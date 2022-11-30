// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_THUMBNAIL_H_
#define COMPONENTS_HISTORY_CORE_TEST_THUMBNAIL_H_

namespace gfx {
class Image;
}

namespace history {

// Returns a gfx::Image corresponding to kGoogleThumbnail data for test.
gfx::Image CreateGoogleThumbnailForTest();

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_THUMBNAIL_H_
