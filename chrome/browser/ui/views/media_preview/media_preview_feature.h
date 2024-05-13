// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_FEATURE_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_FEATURE_H_

class GURL;
namespace content {
class BrowserContext;
}

namespace media_preview_metrics {
enum class UiLocation;
}

namespace media_preview_feature {

// Returns true if camera and mic previews should be shown in the camera/mic
// permission bubble and page info for the given profile, requesting origin, and
// embedding (top-level) origin.
//
// TODO(crbug.com/335672563): Pass url::Origin, not GURL for origin values.
bool ShouldShowMediaPreview(content::BrowserContext& browser_context,
                            const GURL& requesting_origin_url,
                            const GURL& embedding_origin_url,
                            media_preview_metrics::UiLocation ui_location);

}  // namespace media_preview_feature

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_FEATURE_H_
