// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::{CxxString, CxxVector};
use std::pin::Pin;

#[cxx::bridge(namespace = "base32::rust")]
mod ffi {
    // LINT.IfChange(Base32EncodePolicy)
    /// Policy for trailing padding in the output of `base32_encode`.
    enum Base32EncodePolicy {
        /// Include trailing padding in the output when necessary.
        IncludePadding,

        /// Omit trailing padding in the output. Such an output will not be
        /// decodable unless the input size is known by the decoder. The size of
        /// such an output is guaranteed to be
        /// `(input_size * 8.0 / 5.0).ceil()`.
        OmitPadding,
    }
    // LINT.ThenChange(//components/base32/base32.h:Base32EncodePolicy)

    extern "Rust" {
        fn base32_encode(input: &[u8], policy: Base32EncodePolicy, mut out: Pin<&mut CxxString>);

        #[must_use]
        fn base32_decode(input: &[u8], mut out: Pin<&mut CxxVector<u8>>) -> bool;
    }
}

const ENCODE_CHUNK: usize = 5;

static ENCODING: &[u8; 32] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

// The implementation below assumes that the encoding chunk size is less than
// the number of bits in a byte.
const _: () = assert!(ENCODE_CHUNK < (u8::BITS as usize));

struct Base32EncodeIter<'s> {
    slice: &'s [u8],
    bit_index: usize,
}

/// Implements the encoding procedure by iterating over the input slice 5 bits
/// at a time, translating each chunk of 5 bits into 1 encoded character. Per
/// RFC 4648, at the end of the bit stream, the last chunk is zero-padded to 5
/// bits on the right.
impl Iterator for Base32EncodeIter<'_> {
    type Item = u8;

    fn next(&mut self) -> Option<Self::Item> {
        // Extract up to 5 bits from the current byte.
        let byte: &u8 = self.slice.first()?;
        let mut encoded: u8 = (byte << self.bit_index) >> (8 - ENCODE_CHUNK);
        self.bit_index += ENCODE_CHUNK;
        // If we didn't get the full 5 bits, copy remaining bits from the
        // next byte.
        if self.bit_index >= 8 {
            self.bit_index -= 8;
            self.slice = &self.slice[1..];
            if self.bit_index > 0 {
                let next_byte: &u8 = self.slice.first().unwrap_or(&0u8);
                encoded |= next_byte >> (8 - self.bit_index);
            }
        }
        ENCODING.get(encoded as usize).copied()
    }

    /// Computes the remaining number of encoded output chars that would be
    /// produced from this iterator, excluding trailing padding '=' characters.
    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = if self.slice.is_empty() {
            0
        } else {
            let cur_byte_bits = 8 - self.bit_index;
            let rest_bytes = self.slice.len() - 1;
            // Compute the number of bits not included in any full chunks of 40
            // input bits (= 5 input bytes).
            let remainder_bits = cur_byte_bits + (rest_bytes % ENCODE_CHUNK) * 8;
            // The total length is made up of full chunks (40 input bits -> 8
            // output chars) plus at most one potentially partial (<= 8-char)
            // chunk from the remainder bits.
            (rest_bytes / ENCODE_CHUNK) * 8 + remainder_bits.div_ceil(ENCODE_CHUNK)
        };
        (len, Some(len))
    }
}

impl ExactSizeIterator for Base32EncodeIter<'_> {}

/// Encodes `input` using the base32 encoding specified in RFC 4648. Writes the
/// encoded string to `out`, which should be passed empty.
pub fn base32_encode(input: &[u8], policy: ffi::Base32EncodePolicy, mut out: Pin<&mut CxxString>) {
    let encode_iter = Base32EncodeIter { slice: input, bit_index: 0 };

    // Compute the length of the encoded output. Per RFC 4648, each 40-bit chunk
    // (= 5-byte chunk) of input data gets encoded as 8 output characters, where
    // each output character encodes 5 bits of the input data (or fewer than 5,
    // for the last output character).
    let encoded_char_length = match policy {
        // When using padding, the last chunk's worth of encoded output is
        // padded to 8 output characters using the '=' padding character, even
        // if the corresponding input is fewer than 40 bits. This has an
        // equivalent effect on the output length as padding the input to the
        // nearest full 5-byte chunk.
        ffi::Base32EncodePolicy::IncludePadding => input.len().div_ceil(ENCODE_CHUNK) * 8,
        ffi::Base32EncodePolicy::OmitPadding => encode_iter.len(),
        _ => unreachable!(),
    };

    out.as_mut().clear();
    out.as_mut().reserve(encoded_char_length);
    for byte in encode_iter {
        out.as_mut().push_bytes(&[byte]);
    }
    let padding_count = encoded_char_length.saturating_sub(out.len());
    out.as_mut().push_bytes(&b"======"[..padding_count]);
}

/// Decodes `input` from base32. Writes the decoded bytes to `out`, which should
/// be passed empty. Returns true if decoding was successful, or false on error.
/// If this returns false, the contents of `out` should be disregarded.
#[must_use]
pub fn base32_decode(input: &[u8], mut out: Pin<&mut CxxVector<u8>>) -> bool {
    // Remove padding, if present, by truncating at the first padding char.
    // Note: Since the preimage must have been a multiple of 8 bits, the only valid
    // possibilities are 6, 4, 3, or 1 trailing padding characters. The C++
    // implementation permits arbitrary numbers of trailing padding characters,
    // as well as non-padding chars after the first padding char; this
    // implementation matches that behavior.
    let trimmed_input = match input.iter().position(|&ch| ch == b'=') {
        Some(pos) => &input[..pos],
        None => input,
    };
    let chunks_to_decode = trimmed_input.chunks(8);
    out.as_mut().reserve(chunks_to_decode.len() * ENCODE_CHUNK);
    for chunk in chunks_to_decode {
        if base32_decode_chunk(chunk, out.as_mut()).is_err() {
            return false;
        }
    }
    true
}

struct InvalidCharacter;

/// Decodes a single chunk of up to 8 bytes, appending the decoded bytes (up to
/// 5 bytes) to `acc`.
fn base32_decode_chunk(
    chunk_bytes: &[u8],
    mut acc: Pin<&mut CxxVector<u8>>,
) -> Result<(), InvalidCharacter> {
    // Each encoded character represents 5 bits of the preimage. Except for
    // possibly the last chunk, this should be 40 bits.
    let preimage_bit_length = chunk_bytes.len() * ENCODE_CHUNK;
    // Compute the number of bytes of the original input chunk this function
    // should return. We assume the preimage consisted of a whole number of
    // bytes of input (i.e. a multiple of 8 bits), padded up to the nearest
    // multiple of 5 bits during encoding, which may have added extra bits.
    // Those (preimage_bits % 8) padding bits will be discarded. (A stricter
    // decoder would enforce that they are all zero, but this currently does
    // not.)
    let decoded_byte_length = preimage_bit_length / 8;

    // `chunk_bytes` is never empty. This ensures that the use of <<= below
    // never tries to shift a u64 by 64 bits, which would be UB.
    debug_assert!(preimage_bit_length > 0);
    debug_assert!(preimage_bit_length <= 8 * ENCODE_CHUNK);
    // Collect the preimage bits into a u64 and then spit them back out as the
    // appropriate groups of 8.
    let mut decoded = 0u64;
    for input_char in chunk_bytes {
        decoded <<= ENCODE_CHUNK;
        let new_bits: u8 = base32_reverse_mapping(*input_char)?;
        decoded |= new_bits as u64;
    }
    // Shift the populated bits to the beginning.
    decoded <<= 64 - preimage_bit_length;
    acc.as_mut().extend(decoded.to_be_bytes().into_iter().take(decoded_byte_length));
    Ok(())
}

/// Returns the 5-bit number matching the provided encoded character.
fn base32_reverse_mapping(input_char: u8) -> Result<u8, InvalidCharacter> {
    match input_char {
        b'A'..=b'Z' => Ok(input_char - b'A'),
        b'2'..=b'7' => Ok(input_char - b'2' + 26),
        _ => Err(InvalidCharacter),
    }
}
