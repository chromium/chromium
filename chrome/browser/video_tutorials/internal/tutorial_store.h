// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_STORE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/internal/store.h"
#include "chrome/browser/video_tutorials/internal/tutorial_group.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

void DataToProto(video_tutorials::TutorialGroup* data,
                 video_tutorials::proto::VideoTutorialGroup* proto);
void ProtoToData(video_tutorials::proto::VideoTutorialGroup* proto,
                 video_tutorials::TutorialGroup* data);

}  // namespace leveldb_proto

namespace video_tutorials {
// Persist layer of video tutorial groups.
class TutorialStore : public Store<TutorialGroup> {
 public:
  using TutorialProtoDb = std::unique_ptr<
      leveldb_proto::ProtoDatabase<video_tutorials::proto::VideoTutorialGroup,
                                   TutorialGroup>>;
  explicit TutorialStore(TutorialProtoDb db);
  ~TutorialStore() override;

  TutorialStore(const TutorialStore& other) = delete;
  TutorialStore& operator=(const TutorialStore& other) = delete;

 private:
  using KeyEntryVector = std::vector<std::pair<std::string, TutorialGroup>>;
  using KeyVector = std::vector<std::string>;
  using EntryVector = std::vector<TutorialGroup>;

  // Store<TutorialGroup> implementation.
  void Initialize(SuccessCallback callback) override;
  void LoadEntries(const std::vector<std::string>& keys,
                   LoadEntriesCallback callback) override;
  void UpdateAll(
      const std::vector<std::pair<std::string, TutorialGroup>>& key_entry_pairs,
      const std::vector<std::string>& keys_to_delete,
      UpdateCallback callback) override;

  // Called when db is initialized.
  void OnDbInitialized(SuccessCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  TutorialProtoDb db_;

  base::WeakPtrFactory<TutorialStore> weak_ptr_factory_{this};
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_STORE_H_
