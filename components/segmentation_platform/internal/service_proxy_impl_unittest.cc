// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/service_proxy_impl.h"

#include "base/strings/string_number_conversions.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {
// Adds a segment info into a map, and return a copy of it.
proto::SegmentInfo AddSegmentInfo(
    std::map<std::string, proto::SegmentInfo>* db_entries,
    OptimizationTarget segment_id) {
  proto::SegmentInfo info;
  info.set_segment_id(segment_id);
  db_entries->insert(
      std::make_pair(base::NumberToString(static_cast<int>(segment_id)), info));
  return info;
}

}  // namespace

class ServiceProxyImplTest : public testing::Test,
                             public ServiceProxy::Observer {
 public:
  ServiceProxyImplTest() = default;
  ~ServiceProxyImplTest() override = default;

  void SetUpProxy() {
    DCHECK(!db_);
    DCHECK(!segment_db_);
    auto db = std::make_unique<leveldb_proto::test::FakeDB<proto::SegmentInfo>>(
        &db_entries_);
    db_ = db.get();
    segment_db_ = std::make_unique<SegmentInfoDatabase>(std::move(db));

    service_proxy_impl_ =
        std::make_unique<ServiceProxyImpl>(nullptr, segment_db_.get());
    service_proxy_impl_->AddObserver(this);
  }

  void TearDown() override {
    db_entries_.clear();
    db_ = nullptr;
    segment_db_.reset();
  }

  void OnServiceStatusChanged(bool is_initialized, int status_flag) override {
    is_initialized_ = is_initialized;
    status_flag_ = status_flag;
  }

  void OnSegmentInfoAvailable(
      const std::vector<std::string>& segment_info) override {
    segment_info_ = segment_info;
  }

 protected:
  bool is_initialized_ = false;
  int status_flag_ = 0;

  std::map<std::string, proto::SegmentInfo> db_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::SegmentInfo>> db_{nullptr};
  std::unique_ptr<SegmentInfoDatabase> segment_db_;
  std::unique_ptr<ServiceProxyImpl> service_proxy_impl_;
  std::vector<std::string> segment_info_;
};

TEST_F(ServiceProxyImplTest, GetServiceStatus) {
  SetUpProxy();
  service_proxy_impl_->GetServiceStatus();
  ASSERT_EQ(is_initialized_, false);
  ASSERT_EQ(status_flag_, 0);

  service_proxy_impl_->OnServiceStatusChanged(false, 1);
  ASSERT_EQ(is_initialized_, false);
  ASSERT_EQ(status_flag_, 1);

  service_proxy_impl_->OnServiceStatusChanged(true, 7);
  ASSERT_EQ(is_initialized_, true);
  ASSERT_EQ(status_flag_, 7);

  db_->LoadCallback(true);
  ASSERT_TRUE(segment_info_.empty());
}

TEST_F(ServiceProxyImplTest, GetSegmentationInfoFromDB) {
  proto::SegmentInfo info = AddSegmentInfo(
      &db_entries_,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);
  db_->LoadCallback(true);
  ASSERT_EQ(segment_info_.size(), 1u);
  ASSERT_EQ(segment_info_.at(0), ServiceProxyImpl::SegmentInfoToString(info));
}

}  // namespace segmentation_platform
