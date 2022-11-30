// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_TEST_FACTORY_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_TEST_FACTORY_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"

namespace paint_preview {

// An approximation of a SimpleKeyedServiceFactory for a keyed
// PaintPerviewBaseService. This is different than a "real" version as it
// uses base::NoDestructor rather than base::Singleton due to testing object
// lifecycle.
class PaintPreviewBaseServiceTestFactory : public SimpleKeyedServiceFactory {
 public:
  static PaintPreviewBaseServiceTestFactory* GetInstance();

  static PaintPreviewBaseService* GetForKey(SimpleFactoryKey* key);

  // The following are unusual methods for an implementer of a
  // KeyedServiceFactory. They are intended to help with testing.

  static std::unique_ptr<KeyedService> Build(SimpleFactoryKey* key);

  void Destroy(SimpleFactoryKey* key);

  // These are public due to using base::NoDestructor. Don't call directly.
  PaintPreviewBaseServiceTestFactory();
  ~PaintPreviewBaseServiceTestFactory() override;

  PaintPreviewBaseServiceTestFactory(
      const PaintPreviewBaseServiceTestFactory&) = delete;
  PaintPreviewBaseServiceTestFactory& operator=(
      const PaintPreviewBaseServiceTestFactory&) = delete;

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;

  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_TEST_FACTORY_H_
