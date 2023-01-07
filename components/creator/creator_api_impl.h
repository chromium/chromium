// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREATOR_CREATOR_API_IMPL_H_
#define COMPONENTS_CREATOR_CREATOR_API_IMPL_H_

#include "components/creator/public/creator_api.h"

namespace creator {
class CreatorApiImpl : public CreatorApi {
 public:
  CreatorApiImpl();
  ~CreatorApiImpl() override;
  CreatorApiImpl(const CreatorApiImpl&) = delete;
  CreatorApiImpl& operator=(const CreatorApiImpl&) = delete;

  // Creator
  Creator GetCreator(std::string web_channel_id);
};

}  // namespace creator

#endif  // COMPONENTS_CREATOR_CREATOR_API_IMPL_H_
