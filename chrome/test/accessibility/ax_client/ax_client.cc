// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/ax_client/ax_client.h"

#include <utility>

#include "base/notimplemented.h"
#include "base/process/process.h"
#include "base/win/windows_handle_util.h"
#include "chrome/test/accessibility/ax_client/ax_client_ia2.h"
#include "chrome/test/accessibility/ax_client/ax_client_uia.h"

namespace ax_client {

namespace {

// Returns a new instance of a the AxClientImpl that implements `client_api`.
std::unique_ptr<AxClientImpl> CreateImpl(AxClient::ClientApi client_api) {
  switch (client_api) {
    case AxClient::ClientApi::kUiAutomation:
      return std::make_unique<AxClientUia>();
    case AxClient::ClientApi::kIAccessible2:
      return std::make_unique<AxClientIa2>();
  }
}

}  // namespace

AxClient::AxClient(ClientApi client_api, base::OnceClosure on_destroyed)
    : on_destroyed_(std::move(on_destroyed)), impl_(CreateImpl(client_api)) {}

AxClient::~AxClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  impl_.reset();

  if (on_destroyed_) {
    std::move(on_destroyed_).Run();
  }
}

void AxClient::Initialize(uint32_t hwnd, InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  HWND the_window = reinterpret_cast<HWND>(base::win::Uint32ToHandle(hwnd));
  std::move(callback).Run(impl_->Initialize(the_window));
}

void AxClient::FindAll(FindAllCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::move(callback).Run(impl_->FindAll());
}

void AxClient::Shutdown(ShutdownCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  impl_->Shutdown();
  std::move(callback).Run();
}

void AxClient::Terminate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::Process::TerminateCurrentProcessImmediately(1);
}

}  // namespace ax_client
