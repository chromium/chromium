// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_TEST_DATA_SOURCE_DELEGATE_H_
#define COMPONENTS_EXO_TEST_TEST_DATA_SOURCE_DELEGATE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/test/test_security_delegate.h"

namespace exo::test {

// A test implementation of DataSourceDelegate which can be set up with data to
// be used in tests.
class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  TestDataSourceDelegate();
  ~TestDataSourceDelegate() override;

  TestDataSourceDelegate(const TestDataSourceDelegate&) = delete;
  TestDataSourceDelegate& operator=(const TestDataSourceDelegate&) = delete;

  // DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* device) override {}
  void OnTarget(const std::optional<std::string>& mime_type) override {}
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override;
  void OnCancelled() override;
  void OnDndDropPerformed() override {}
  void OnDndFinished() override {}
  void OnAction(DndAction dnd_action) override {}
  bool CanAcceptDataEventsForSurface(Surface* surface) const override;
  SecurityDelegate* GetSecurityDelegate() const override;

  bool cancelled() const { return cancelled_; }
  void set_can_accept(bool can_accept) { can_accept_ = can_accept; }
  void SetData(const std::string& mime_type, std::string data);

 private:
  bool cancelled_ = false;
  bool can_accept_ = true;
  base::flat_map<std::string, std::string> data_map_;
  std::unique_ptr<SecurityDelegate> security_delegate_ =
      std::make_unique<TestSecurityDelegate>();
};

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_TEST_DATA_SOURCE_DELEGATE_H_
