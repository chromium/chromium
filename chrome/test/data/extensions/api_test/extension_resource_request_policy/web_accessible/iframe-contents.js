// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We can't use the onload handler for the iframe (that fires for denied loads
// too). To make sure that right iframe actually loaded, we signal the parent.
window.top.iframeLoaded();
