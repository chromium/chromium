// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_ISOLATED_WORLD_UTIL_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_ISOLATED_WORLD_UTIL_H_

namespace translate {

// Ensures the isolated world information for |world_id| is initialized.
void EnsureIsolatedWorldInitialized(int world_id);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_ISOLATED_WORLD_UTIL_H_
