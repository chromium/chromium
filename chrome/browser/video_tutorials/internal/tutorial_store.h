// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_STORE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_STORE_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/video_tutorials/internal/proto_conversions.h"
#include "chrome/browser/video_tutorials/internal/store.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace video_tutorials {
// Persistence layer for storing the video tutorial metadata.
class TutorialStore : public Store<proto::VideoTutorialGroups> {
 public:
  using TutorialProtoDb = std::unique_ptr<leveldb_proto::ProtoDatabase<
      video_tutorials::proto::VideoTutorialGroups>>;
  explicit TutorialStore(TutorialProtoDb db);
  ~TutorialStore() override;

  TutorialStore(const TutorialStore& other) = delete;
  TutorialStore& operator=(const TutorialStore& other) = delete;

  // Store implementation.
  void InitAndLoad(LoadCallback callback) override;
  void Update(const proto::VideoTutorialGroups& entry,
              UpdateCallback callback) override;

 private:
  // Called when the db is initialized.
  void OnDbInitialized(LoadCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  // The underlying database.
  TutorialProtoDb db_;

  // The database initialization status.
  absl::optional<bool> init_success_;

  base::WeakPtrFactory<TutorialStore> weak_ptr_factory_{this};
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_STORE_H_
