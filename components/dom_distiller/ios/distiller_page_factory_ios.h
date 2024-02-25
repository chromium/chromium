// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_FACTORY_IOS_H_
#define COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_FACTORY_IOS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/dom_distiller/core/distiller_page.h"

namespace web {
class BrowserState;
}

namespace dom_distiller {

// DistillerPageFactoryIOS is an iOS-specific implementation of the
// DistillerPageFactory interface allowing the creation of DistillerPage
// instances.
class DistillerPageFactoryIOS : public DistillerPageFactory {
 public:
  explicit DistillerPageFactoryIOS(web::BrowserState* browser_state);

  DistillerPageFactoryIOS(const DistillerPageFactoryIOS&) = delete;
  DistillerPageFactoryIOS& operator=(const DistillerPageFactoryIOS&) = delete;

  ~DistillerPageFactoryIOS() override;

  // Implementation of DistillerPageFactory:
  std::unique_ptr<DistillerPage> CreateDistillerPage(
      const gfx::Size& view_size) const override;
  std::unique_ptr<DistillerPage> CreateDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) const override;

 private:
  raw_ptr<web::BrowserState> browser_state_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_FACTORY_IOS_H_
