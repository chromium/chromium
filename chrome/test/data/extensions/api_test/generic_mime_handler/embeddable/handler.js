// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Claim the stream and keep the OOPIF alive. The permission embedding
// test only inspects the frame tree, so this handler does no work
// beyond binding to the stream.
chrome.mimeHandler.getStreamInfo();
