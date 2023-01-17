// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/creator/creator_api_impl.h"

#include <string>

#include "components/creator/public/creator_api.h"
#include "url/gurl.h"

namespace creator {
Creator CreatorApiImpl::GetCreator(std::string channel_id) {
  // TODO(crbug.com/1365645) query actual data for given channel id.
  return Creator{u"alexainsley.com", /*title=*/u"Alex Ainsley"};
}

void CreatorApiImpl::GetWebId(std::string url,
                              base::OnceCallback<void(std::string)> callback) {
  if (url.empty()) {
    std::move(callback).Run({});
    return;
  }
}
}  // namespace creator