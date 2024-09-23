// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PRIVILEGED_MOJOM_BUCKET_CLIENT_INFO_MOJOM_TRAITS_H_
#define COMPONENTS_SERVICES_STORAGE_PRIVILEGED_MOJOM_BUCKET_CLIENT_INFO_MOJOM_TRAITS_H_

#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/bucket_client_info.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<storage::mojom::BucketClientInfoDataView,
                   storage::BucketClientInfo> {
 public:
  static int32_t process_id(const storage::BucketClientInfo& info) {
    return info.process_id;
  }
  static const blink::ExecutionContextToken& context_token(
      const storage::BucketClientInfo& info) {
    return info.context_token;
  }
  static const std::optional<blink::DocumentToken>& document_token(
      const storage::BucketClientInfo& info) {
    return info.document_token;
  }

  static bool Read(storage::mojom::BucketClientInfoDataView data,
                   storage::BucketClientInfo* out);
};

}  // namespace mojo

#endif  // COMPONENTS_SERVICES_STORAGE_PRIVILEGED_MOJOM_BUCKET_CLIENT_INFO_MOJOM_TRAITS_H_
