// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use core::cmp::Ordering;

use crate::constants::*;
use crate::writer;

/// Value represents a CBOR structure.
///
/// Integers are mapped to `i64` despite CBOR having 65-bit integers. CBOR
/// integers outside the range of an `i64` result in an error during parsing.
/// Byte strings are returned as `Bytes`s in order to avoid copies.
#[derive(Debug, PartialEq, Clone)]
pub enum Value {
    Int(i64),
    Bytestring(Vec<u8>),
    String(String),
    Array(Vec<Value>),
    Map(BTreeMap<MapKey, Value>),
    Boolean(bool),
    Float(f64),
    Null,
    Undefined,
    InvalidUtf8(Vec<u8>),
}

impl Value {
    // to_bytes serialises `self` to CBOR and returns the result.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut ret = Vec::new();
        self.append_bytes(&mut ret);
        ret
    }

    // append_bytes appends a serialisation of `self` to `out`.
    pub fn append_bytes(&self, out: &mut Vec<u8>) {
        writer::append_value(self, out);
    }
}

/// A MapKey is the type of values that can key a CBOR map.
#[derive(Debug, PartialEq, Eq, Clone)]
pub enum MapKey {
    // A separate `MapKey` type is used because we want to exclude things like
    // maps keyed by arrays or other maps. Such structures never appear in
    // CTAP and so we don't need to support them.
    //
    // We expect that a map will always have keys of the same type, which
    // suggests that `Value::Map` could be split into `Value::IntKeyedMap` etc.
    // However, that falls down when a map is empty because the parser can't
    // know what the key type should be, yet calling code will want to expect
    // the right type of map. Thus we end up supporting heterogeneous maps.
    Int(i64),
    Bytestring(Vec<u8>),
    String(String),
}

impl MapKey {
    pub(crate) fn type_arg_and_payload(&self) -> (u8, u64, Option<&[u8]>) {
        match self {
            MapKey::Int(v) if *v >= 0 => (MAJOR_TYPE_UNSIGNED_INT, *v as u64, None),
            MapKey::Int(v) => (MAJOR_TYPE_NEGATIVE_INT, !*v as u64, None),
            MapKey::Bytestring(b) => (MAJOR_TYPE_BYTE_STRING, b.len() as u64, Some(b)),
            MapKey::String(s) => (MAJOR_TYPE_TEXT_STRING, s.len() as u64, Some(s.as_bytes())),
        }
    }
}

impl PartialOrd for MapKey {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for MapKey {
    fn cmp(&self, other: &Self) -> Ordering {
        self.type_arg_and_payload().cmp(&other.type_arg_and_payload())
    }
}
