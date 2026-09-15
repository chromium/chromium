// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
