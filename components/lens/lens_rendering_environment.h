// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_RENDERING_ENVIRONMENT_H_
#define COMPONENTS_LENS_LENS_RENDERING_ENVIRONMENT_H_

namespace lens {

// Lens Web rendering environments
enum RenderingEnvironment {
  ONELENS_DESKTOP_WEB_CHROME_SIDE_PANEL,
  ONELENS_DESKTOP_WEB_FULLSCREEN,
  ONELENS_AMBIENT_VISUAL_SEARCH_WEB_FULLSCREEN,
  CHROME_SEARCH_COMPANION,
  RENDERING_ENV_UNKNOWN,
};

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_RENDERING_ENVIRONMENT_H_
