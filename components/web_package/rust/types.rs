// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::string::String;
use alloc::vec::Vec;

#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct ParseError {
    pub is_version_error: bool,
    pub message: &'static str,
}

impl ParseError {
    pub(crate) const fn format(message: &'static str) -> Self {
        Self { is_version_error: false, message }
    }

    pub(crate) const fn version(message: &'static str) -> Self {
        Self { is_version_error: true, message }
    }
}

#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct MagicAndVersionResult {
    pub section_lengths_len: u64,
    pub next_read_offset: u64,
    pub next_read_length: u64,
}

#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct SectionOffsetEntry {
    pub name: String,
    pub offset: u64,
    pub length: u64,
}

#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct BundleHeaderResult {
    pub metadata_sections: Vec<SectionOffsetEntry>,
    pub responses_offset: u64,
    pub responses_length: u64,
}

#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ParsedIndexEntry<'a> {
    pub url: &'a str,
    pub offset: u64,
    pub length: u64,
}

/// A single HTTP header field, as raw bytes: `field-value` admits `obs-text`
/// (`%x80-FF`), which RFC 9110 Section 5.5 directs recipients to treat as
/// opaque data.
///
/// https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5
#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct HeaderEntry<'a> {
    pub name: &'a [u8],
    pub value: &'a [u8],
}

#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ResponseParseResult<'a> {
    pub needs_more_data: bool,
    pub required_buffer_size: u64,
    pub response_code: i32,
    pub headers: Vec<HeaderEntry<'a>>,
    pub payload_offset: u64,
    pub payload_length: u64,
}

impl<'a> ResponseParseResult<'a> {
    pub(crate) fn needs_more(required_buffer_size: u64) -> Self {
        Self {
            needs_more_data: true,
            required_buffer_size,
            response_code: 0,
            headers: Vec::new(),
            payload_offset: 0,
            payload_length: 0,
        }
    }

    pub(crate) fn success(
        response_code: i32,
        headers: Vec<HeaderEntry<'a>>,
        payload_offset: u64,
        payload_length: u64,
    ) -> Self {
        Self {
            needs_more_data: false,
            required_buffer_size: 0,
            response_code,
            headers,
            payload_offset,
            payload_length,
        }
    }
}
