// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::string::String;
use alloc::vec::Vec;

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SignatureType {
    Ed25519 = 0,
    EcdsaP256SHA256 = 1,
    Unknown = 2,
}

#[repr(C)]
#[derive(Debug, PartialEq, Clone)]
pub struct SignatureStackEntry<'a> {
    pub attributes_cbor: Vec<u8>,
    pub signature_type: SignatureType,
    pub public_key: &'a [u8],
    pub signature: &'a [u8],
}

#[repr(C)]
#[derive(Debug, PartialEq, Clone)]
pub struct IntegrityBlock<'a> {
    pub size: usize,
    pub web_bundle_id: &'a str,
    pub attributes_cbor: Vec<u8>,
    pub signature_stack: Vec<SignatureStackEntry<'a>>,
}

#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ParseError {
    pub is_version_error: bool,
    pub message: String,
}

impl ParseError {
    pub(crate) fn format(message: impl Into<String>) -> Self {
        Self { is_version_error: false, message: message.into() }
    }

    pub(crate) fn version(message: impl Into<String>) -> Self {
        Self { is_version_error: true, message: message.into() }
    }
}
