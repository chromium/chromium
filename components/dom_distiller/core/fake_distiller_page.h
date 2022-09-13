// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_PAGE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_PAGE_H_

#include "components/dom_distiller/core/distiller_page.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dom_distiller {
namespace test {

class MockDistillerPageFactory : public DistillerPageFactory {
 public:
  MockDistillerPageFactory();
  ~MockDistillerPageFactory() override;
  MOCK_CONST_METHOD0(CreateDistillerPageImpl, DistillerPage*());
  std::unique_ptr<DistillerPage> CreateDistillerPage(
      const gfx::Size& render_view_size) const override {
    return std::unique_ptr<DistillerPage>(CreateDistillerPageImpl());
  }
  std::unique_ptr<DistillerPage> CreateDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) const override {
    return std::unique_ptr<DistillerPage>(CreateDistillerPageImpl());
  }
};

class MockDistillerPage : public DistillerPage {
 public:
  MockDistillerPage();
  ~MockDistillerPage() override;
  bool StringifyOutput() override { return false; }
  MOCK_METHOD2(DistillPageImpl,
               void(const GURL& gurl, const std::string& script));
};

}  // namespace test
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_PAGE_H_
