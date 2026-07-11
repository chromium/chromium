// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `cbor` implements the subset of CBOR used in [CTAP2][1], with optional
//! extensions to support Chromium-specific requirements.
//!
//! Note: The core of this parser was originally imported from
//! `//third_party/cloud_authenticator/cbor`. It was subsequently modified
//! to support Chromium CBOR requirements. It will be wrapped in FFI and
//! eventually replace the current C++ parser.
//!
//! Under the strict CTAP2 configuration (`Config::default()`), parsing is
//! injective, meaning that no two distinct byte strings (that parse
//! successfully) will result in the same value. Reserialising the result of
//! parse will always result in exactly the same byte string. (This is checked
//! by fuzzing.)
//!
//! In the other direction, serialisation is also injective. However, an
//! arbitrary limit is placed on the maximum depth of parsed structures to avoid
//! degenerate inputs from consuming too much stack. Thus structures that are
//! deeper than this will serialise but that result cannot be parsed back to
//! the same value. Aside from this exception, all values should round-trip
//! correctly under the CTAP2 configuration.
//!
//! Note on Relaxations: When `Config::allow_floating_point` or
//! `Config::allow_invalid_utf8` are enabled, the injectivity and exact
//! round-tripping guarantees no longer hold. For example, `f16` floats will
//! parse to `f64` and serialize back as `f64` (changing the byte string).
//!
//! ```
//! let value = cbor::Value::String("hello".to_string());
//! let serialized = value.to_bytes();
//! assert_eq!(serialized, vec![0x65u8, 0x68, 0x65, 0x6c, 0x6c, 0x6f]);
//! assert_eq!(cbor::parse_with_config(serialized, cbor::Config::default()), Ok((value, 1)));
//! ```
//!
//! [1]: https://fidoalliance.org/specs/fido-v2.2-rd-20230321/fido-client-to-authenticator-protocol-v2.2-rd-20230321.html#ctap2-canonical-cbor-encoding-form

#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

mod constants;
mod float_conversions;
mod reader;
mod values;
mod writer;

pub use constants::MAX_DEPTH;
pub use reader::{parse_with_config, Config, Error};
pub use values::{MapKey, Value};
pub use writer::write;

// This code assumes that `usize` fits in a `u64` because it uses `as u64` in a
// couple of places.
const _: () =
    assert!(core::mem::size_of::<usize>() <= core::mem::size_of::<u64>(), "usize too large");
