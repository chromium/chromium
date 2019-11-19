// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/simple_factory_key.h"

#include "components/keyed_service/core/simple_dependency_manager.h"

SimpleFactoryKey::SimpleFactoryKey(const base::FilePath& path,
                                   bool is_off_the_record)
    : path_(path), is_off_the_record_(is_off_the_record) {
  SimpleDependencyManager::GetInstance()->MarkContextLive(this);
}

SimpleFactoryKey::~SimpleFactoryKey() = default;
