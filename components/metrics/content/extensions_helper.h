// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_EXTENSIONS_HELPER_H_
#define COMPONENTS_METRICS_CONTENT_EXTENSIONS_HELPER_H_

namespace content {
class RenderProcessHost;
}

namespace metrics {

// ExtensionsHelper is used by ContentStabilityMetricsProvider to determine
// if a RenderProcessHost hosts an extension. This is separate from
// ContentStabilityMetricsProvider to avoid this code depending directly on
// extensions.
class ExtensionsHelper {
 public:
  virtual ~ExtensionsHelper() = default;

  // Returns true if |render_process_host| is hosting an extension.
  virtual bool IsExtensionProcess(
      content::RenderProcessHost* render_process_host) = 0;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_EXTENSIONS_HELPER_H_
