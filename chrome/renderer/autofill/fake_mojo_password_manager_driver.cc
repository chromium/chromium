// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/fake_mojo_password_manager_driver.h"

#include <utility>

FakeMojoPasswordManagerDriver::FakeMojoPasswordManagerDriver() = default;

FakeMojoPasswordManagerDriver::~FakeMojoPasswordManagerDriver() = default;

void FakeMojoPasswordManagerDriver::BindReceiver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void FakeMojoPasswordManagerDriver::Flush() {
  receiver_.FlushForTesting();
}
