// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_DISPLAY_OVERRIDE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_DISPLAY_OVERRIDE_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace base {
class Value;
}

namespace web_app {

// Represents an item in the display override list from the web app manifest.
class DisplayOverride {
 public:
  static DisplayOverride Create(blink::mojom::DisplayMode display_mode);
  static DisplayOverride CreateUnframed(
      std::vector<blink::SafeUrlPattern> url_patterns);

  DisplayOverride(const DisplayOverride&);
  DisplayOverride(DisplayOverride&&);
  ~DisplayOverride();

  DisplayOverride& operator=(const DisplayOverride&);
  DisplayOverride& operator=(DisplayOverride&&);

  bool operator==(const DisplayOverride& other) const;

  // Parses a proto message. Returns `nullopt` when:
  // - `display_mode` is not set.
  // - `url_patterns` is set and fails to parse.
  // - `url_patterns` is set and `display_mode` is not "unframed".
  static std::optional<DisplayOverride> Parse(
      const proto::WebApp::DisplayOverrideItem& proto);
  proto::WebApp::DisplayOverrideItem ToProto() const;

  base::Value ToDebugValue() const;
  std::string ToString() const;

  blink::mojom::DisplayMode display_mode() const { return display_mode_; }
  const std::vector<blink::SafeUrlPattern>& url_patterns() const {
    return url_patterns_;
  }

 private:
  DisplayOverride(blink::mojom::DisplayMode display_mode,
                  std::vector<blink::SafeUrlPattern> url_patterns);

  // The display mode of this override.
  blink::mojom::DisplayMode display_mode_;

  // The URL patterns where this override should apply. Empty means it applies
  // in all URLs.
  //
  // Currently this can only be non-empty in the "unframed" display mode.
  std::vector<blink::SafeUrlPattern> url_patterns_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_DISPLAY_OVERRIDE_H_
