// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_URL_REWRITE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_URL_REWRITE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_

#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/cast_core/public/src/proto/v2/url_rewrite.pb.h"

namespace mojo {

// This conversion is done with a TypeCoverter rather than a typemap because
// it is only done one way, from the gRPC type to the Mojo type. This conversion
// is only done once, in the browser process. These rules are validated after
// they have been converted into Mojo.
// In Core Runtime, we have a one-way flow from the untrusted embedder into the
// browser process, via a gRPC API. From there, the rules are converted into
// Mojo and then validated before being sent to renderer processes. No further
// conversion is performed, the Mojo types are used as is to apply the rewrites
// on URL requests.
// Converter returns |nullptr| if conversion is not possible.
template <>
struct TypeConverter<url_rewrite::mojom::UrlRequestRewriteRulesPtr,
                     cast::v2::UrlRequestRewriteRules> {
  static url_rewrite::mojom::UrlRequestRewriteRulesPtr Convert(
      const cast::v2::UrlRequestRewriteRules& input);
};

}  // namespace mojo

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_URL_REWRITE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_
