// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service_test_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"

namespace paint_preview {

const char kTestFeatureDir[] = "test_feature";

PaintPreviewBaseServiceTestFactory*
PaintPreviewBaseServiceTestFactory::GetInstance() {
  // Use NoDestructor rather than a singleton due to lifetime behavior in
  // tests.
  static base::NoDestructor<PaintPreviewBaseServiceTestFactory> factory;
  return factory.get();
}

PaintPreviewBaseService* PaintPreviewBaseServiceTestFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<PaintPreviewBaseService*>(
      GetInstance()->GetServiceForKey(key, true));
}

void PaintPreviewBaseServiceTestFactory::Destroy(SimpleFactoryKey* key) {
  SimpleContextShutdown(key);
  SimpleContextDestroyed(key);
}

std::unique_ptr<KeyedService> PaintPreviewBaseServiceTestFactory::Build(
    SimpleFactoryKey* key) {
  return std::make_unique<PaintPreviewBaseService>(
      std::make_unique<PaintPreviewFileMixin>(key->GetPath(), kTestFeatureDir),
      nullptr, key->IsOffTheRecord());
}

PaintPreviewBaseServiceTestFactory::~PaintPreviewBaseServiceTestFactory() =
    default;

PaintPreviewBaseServiceTestFactory::PaintPreviewBaseServiceTestFactory()
    : SimpleKeyedServiceFactory("PaintPreviewBaseService",
                                SimpleDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
PaintPreviewBaseServiceTestFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return Build(key);
}

SimpleFactoryKey* PaintPreviewBaseServiceTestFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

}  // namespace paint_preview
