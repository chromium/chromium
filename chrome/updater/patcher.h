// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PATCHER_H_
#define CHROME_UPDATER_PATCHER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/patcher.h"

namespace updater {

class PatcherFactory : public update_client::PatcherFactory {
 public:
  PatcherFactory();

  scoped_refptr<update_client::Patcher> Create() const override;

 protected:
  ~PatcherFactory() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PatcherFactory);
};

}  // namespace updater

#endif  // CHROME_UPDATER_PATCHER_H_
