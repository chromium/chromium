// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_DELEGATE_FACTORY_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_DELEGATE_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"

namespace chrome_cleaner {

scoped_refptr<EngineDelegate> CreateEngineDelegate(Engine::Name engine);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_DELEGATE_FACTORY_H_
