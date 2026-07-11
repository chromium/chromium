// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::vec::Vec;
use core::ops::Deref;

use crate::constants::*;
use crate::values::{MapKey, Value};

// low_bits_and_length returns the bottom five bits of the initial byte of a
// CBOR value, and the number of bytes that will need to follow it in order to
// encode an argument `arg`.
pub(crate) fn low_bits_and_length(arg: u64) -> (u8, usize) {
    if arg < ADDL_INFO_1_BYTE as u64 {
        (arg as u8, 0)
    } else if arg < 0x100 {
        (ADDL_INFO_1_BYTE, 1)
    } else if arg < 0x10000 {
        (ADDL_INFO_2_BYTES, 2)
    } else if arg < 0x100000000 {
        (ADDL_INFO_4_BYTES, 4)
    } else {
        (ADDL_INFO_8_BYTES, 8)
    }
}

/// write_header appends the [initial byte and "argument"][1] of a CBOR value.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8949#name-specification-of-the-cbor-e
pub(crate) fn write_header(out: &mut Vec<u8>, major_type: u8, arg: u64) {
    let (low_bits, num_bytes) = low_bits_and_length(arg);
    out.push(major_type << 5 | low_bits);
    let value_bytes = arg.to_be_bytes();
    out.extend_from_slice(&value_bytes[8 - num_bytes..]);
}

/// write serialises a Value to a new CBOR byte vector.
pub fn write(val: &Value) -> Vec<u8> {
    let mut ret = Vec::new();
    append_value(val, &mut ret);
    ret
}

/// append_value appends a serialisation of a Value to `out`.
pub(crate) fn append_value(val: &Value, out: &mut Vec<u8>) {
    match val {
        Value::Int(v) if *v >= 0 => write_header(out, MAJOR_TYPE_UNSIGNED_INT, *v as u64),
        Value::Int(v) => write_header(out, MAJOR_TYPE_NEGATIVE_INT, !v as u64),
        Value::Bytestring(s) => {
            write_header(out, MAJOR_TYPE_BYTE_STRING, s.len() as u64);
            out.extend_from_slice(s.deref());
        }
        Value::String(s) => {
            write_header(out, MAJOR_TYPE_TEXT_STRING, s.len() as u64);
            out.extend_from_slice(s.as_bytes());
        }
        Value::Array(a) => {
            write_header(out, MAJOR_TYPE_ARRAY, a.len() as u64);
            for elem in a {
                append_value(elem, out);
            }
        }
        Value::Map(m) => {
            write_header(out, MAJOR_TYPE_MAP, m.len() as u64);
            for (key, value) in m {
                append_map_key(key, out);
                append_value(value, out);
            }
        }
        Value::Boolean(b) => write_header(
            out,
            MAJOR_TYPE_SIMPLE_VALUE,
            if *b { SIMPLE_VALUE_TRUE as u64 } else { SIMPLE_VALUE_FALSE as u64 },
        ),
        Value::Null => write_header(out, MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_NULL as u64),
        Value::Undefined => {
            write_header(out, MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_UNDEFINED as u64)
        }
        Value::InvalidUtf8(bytes) => {
            write_header(out, MAJOR_TYPE_TEXT_STRING, bytes.len() as u64);
            out.extend_from_slice(bytes);
        }
        Value::Float(f) => {
            out.push(MAJOR_TYPE_SIMPLE_VALUE << 5 | SIMPLE_VALUE_FLOAT_64);
            out.extend_from_slice(&f.to_bits().to_be_bytes());
        }
    }
}

/// append_map_key appends a serialisation of a MapKey to `out`.
pub(crate) fn append_map_key(key: &MapKey, out: &mut Vec<u8>) {
    let (major_type, arg, payload) = key.type_arg_and_payload();
    write_header(out, major_type, arg);
    if let Some(payload) = payload {
        out.extend_from_slice(payload);
    }
}
