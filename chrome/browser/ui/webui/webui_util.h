// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "chrome/common/buildflags.h"

struct GritResourceMap;

namespace content {
class WebUIDataSource;
}

namespace webui {

struct LocalizedString;

struct ResourcePath {
  const char* path;
  int id;
};

// Performs common setup steps for |source|, assuming it is using Polymer 3,
// by adding all resources, setting the default resource, setting up i18n,
// and ensuring that tests work correctly by updating the CSP and adding the
// test loader files.
void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources,
                          const std::string& generated_path,
                          int default_resource);

#if BUILDFLAG(OPTIMIZE_WEBUI)
// Same as SetupWebUIDataSource, but for a bundled page; this adds only the
// bundle and the default resource to |source|.
void SetupBundledWebUIDataSource(content::WebUIDataSource* source,
                                 base::StringPiece bundled_path,
                                 int bundle,
                                 int default_resource);
#endif

// Calls content::WebUIDataSource::AddLocalizedString() in a for-loop for
// |strings|. Reduces code size vs. reimplementing the same for-loop.
void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             base::span<const LocalizedString> strings);

// Calls content::WebUIDataSource::AddResourcePath() in a for-loop for |paths|.
// Reduces code size vs. reimplementing the same for-loop.
void AddResourcePathsBulk(content::WebUIDataSource* source,
                          base::span<const ResourcePath> paths);

// AddResourcePathsBulk() variant that works with GritResourceMap.
// Use base::make_span(kResourceMap, kResourceMapSize).
void AddResourcePathsBulk(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources);

// Returns whether the device is enterprise managed. Note that on Linux, there's
// no good way of detecting whether the device is managed, so always return
// false.
bool IsEnterpriseManaged();

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
