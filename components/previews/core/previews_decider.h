// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "components/previews/core/previews_experiments.h"
#include "net/nqe/effective_connection_type.h"

class GURL;

namespace previews {
class PreviewsUserData;

class PreviewsDecider {
 public:
  // Whether |url| is allowed to show a preview of |type|. If the current
  // ECT is strictly faster than |effective_connection_type_threshold|, the
  // preview will be disallowed; preview types that check network quality before
  // calling ShouldAllowPreviewAtECT should pass in
  // EFFECTIVE_CONNECTION_TYPE_4G.
  // |is_server_preview| means that the blacklist does
  // not need to be checked for long term rules when Previews has been
  // configured to allow skipping the blacklist.
  virtual bool ShouldAllowPreviewAtECT(
      PreviewsUserData* previews_data,
      const GURL& url,
      bool is_reload,
      PreviewsType type,
      net::EffectiveConnectionType effective_connection_type_threshold,
      const std::vector<std::string>& host_blacklist_from_finch,
      bool is_server_preview) const = 0;

  // Same as ShouldAllowPreviewAtECT, but uses the previews default
  // EffectiveConnectionType and no blacklisted hosts from the server.
  virtual bool ShouldAllowPreview(PreviewsUserData* previews_data,
                                  const GURL& url,
                                  bool is_reload,
                                  PreviewsType type) const = 0;

  // Whether the |url| is allowed to show a preview of |type|.
  // This only considers whether the URL is constrained/allowed in
  // blacklists/whitelists. It does not include other constraints such
  // as the effective connection type.
  virtual bool IsURLAllowedForPreview(PreviewsUserData* previews_data,
                                      const GURL& url,
                                      PreviewsType type) const = 0;

  // Requests that any applicable detailed resource hints be loaded.
  virtual void LoadResourceHints(const GURL& url) = 0;

  // Logs UMA for whether the OptimizationGuide HintCache has a matching Hint
  // guidance for |url|. This is useful for measuring the effectiveness of the
  // page hints provided by Cacao.
  virtual void LogHintCacheMatch(const GURL& url, bool is_committed) const = 0;

 protected:
  PreviewsDecider() {}
  virtual ~PreviewsDecider() {}
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_
