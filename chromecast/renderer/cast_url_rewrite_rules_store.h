// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_URL_REWRITE_RULES_STORE_H_
#define CHROMECAST_RENDERER_CAST_URL_REWRITE_RULES_STORE_H_

#include "base/memory/scoped_refptr.h"

#include "components/url_rewrite/common/url_request_rewrite_rules.h"

namespace chromecast {

class CastURLRewriteRulesStore {
 public:
  virtual scoped_refptr<url_rewrite::UrlRequestRewriteRules>
  GetUrlRequestRewriteRules(int render_frame_id) const = 0;

 protected:
  virtual ~CastURLRewriteRulesStore() = default;
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_URL_REWRITE_RULES_STORE_H_
