// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/262737383): When Crubit supports generating C++ pattern
// matching and accessor logic for non-repr(C) Rust ADT enums (like `Value` and
// `MapKey`), remove all manual inspection (`kind()`) and payload extraction
// (`as_int()`, `as_string()`, `as_array()`, etc.) methods below, as well as the
// `MapKeyKind` and `ValueKind` proxy enums.
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use core::cmp::Ordering;

use crate::constants::*;
use crate::writer;

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ValueKind {
    Int = 0,
    Bytestring = 1,
    String = 2,
    Array = 3,
    Map = 4,
    Boolean = 5,
    Float = 6,
    Null = 7,
    Undefined = 8,
    InvalidUtf8 = 9,
}

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

    pub fn kind(&self) -> ValueKind {
        match self {
            Value::Int(_) => ValueKind::Int,
            Value::Bytestring(_) => ValueKind::Bytestring,
            Value::String(_) => ValueKind::String,
            Value::Array(_) => ValueKind::Array,
            Value::Map(_) => ValueKind::Map,
            Value::Boolean(_) => ValueKind::Boolean,
            Value::Float(_) => ValueKind::Float,
            Value::Null => ValueKind::Null,
            Value::Undefined => ValueKind::Undefined,
            Value::InvalidUtf8(_) => ValueKind::InvalidUtf8,
        }
    }

    pub fn as_int(&self) -> Option<i64> {
        match self {
            Value::Int(v) => Some(*v),
            _ => None,
        }
    }

    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Value::Boolean(v) => Some(*v),
            _ => None,
        }
    }

    pub fn as_float(&self) -> Option<f64> {
        match self {
            Value::Float(v) => Some(*v),
            _ => None,
        }
    }

    pub fn as_bytestring(&self) -> Option<&[u8]> {
        match self {
            Value::Bytestring(v) => Some(v.as_slice()),
            _ => None,
        }
    }

    pub fn as_string(&self) -> Option<&str> {
        match self {
            Value::String(s) => Some(s.as_str()),
            _ => None,
        }
    }

    pub fn as_invalid_utf8(&self) -> Option<&[u8]> {
        match self {
            Value::InvalidUtf8(v) => Some(v.as_slice()),
            _ => None,
        }
    }

    pub fn as_array(&self) -> Option<&[Value]> {
        match self {
            Value::Array(v) => Some(v.as_slice()),
            _ => None,
        }
    }

    pub fn map_entries(&self) -> Option<Vec<MapEntryRef<'_>>> {
        match self {
            Value::Map(m) => {
                Some(m.iter().map(|(k, v)| MapEntryRef { key: k, value: v }).collect())
            }
            _ => None,
        }
    }
}

impl From<MapKey> for Value {
    fn from(key: MapKey) -> Self {
        match key {
            MapKey::Int(val) => Value::Int(val),
            MapKey::Bytestring(bytes) => Value::Bytestring(bytes),
            MapKey::String(text) => Value::String(text),
        }
    }
}

#[repr(C)]
#[derive(Debug, PartialEq, Clone)]
pub struct MapEntryRef<'a> {
    pub key: &'a MapKey,
    pub value: &'a Value,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MapKeyKind {
    Int = 0,
    Bytestring = 1,
    String = 2,
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

    pub fn kind(&self) -> MapKeyKind {
        match self {
            MapKey::Int(_) => MapKeyKind::Int,
            MapKey::Bytestring(_) => MapKeyKind::Bytestring,
            MapKey::String(_) => MapKeyKind::String,
        }
    }

    pub fn as_int(&self) -> Option<i64> {
        match self {
            MapKey::Int(v) => Some(*v),
            _ => None,
        }
    }

    pub fn as_bytestring(&self) -> Option<&[u8]> {
        match self {
            MapKey::Bytestring(v) => Some(v.as_slice()),
            _ => None,
        }
    }

    pub fn as_string(&self) -> Option<&str> {
        match self {
            MapKey::String(s) => Some(s.as_str()),
            _ => None,
        }
    }
}

impl TryFrom<Value> for MapKey {
    type Error = Value;

    fn try_from(value: Value) -> Result<Self, Value> {
        match value {
            Value::Int(val) => Ok(MapKey::Int(val)),
            Value::Bytestring(bytes) => Ok(MapKey::Bytestring(bytes)),
            Value::String(text) => Ok(MapKey::String(text)),
            _ => Err(value),
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
