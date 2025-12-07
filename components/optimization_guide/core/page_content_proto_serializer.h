// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_PROTO_SERIALIZER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_PROTO_SERIALIZER_H_

namespace url {
class Origin;
}  // namespace url

namespace optimization_guide {

namespace proto {
class SecurityOrigin;
}  // namespace proto

// Prefer using this file (which doesn't depend on content/) for increased
// cross-platform compatibility, unless /content is needed specifically.
//
// A class to serialize a url::Origin into a SecurityOrigin proto. Handles
// opaque origins.
class SecurityOriginSerializer {
 public:
  static void Serialize(
      const url::Origin& origin,
      optimization_guide::proto::SecurityOrigin* proto_origin);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_PROTO_SERIALIZER_H_
