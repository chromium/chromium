// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[repr(C)]
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct ParseError {
    pub message: &'static str,
}

impl ParseError {
    pub(crate) const fn format(message: &'static str) -> Self {
        Self { message }
    }
}
