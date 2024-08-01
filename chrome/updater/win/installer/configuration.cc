// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/win/installer/configuration.h"

#include <shellapi.h>

#include "chrome/updater/win/installer/string.h"

namespace updater {

namespace {

// Returns true if GoogleUpdateIsMachine=1 is present in the environment.
bool GetGoogleUpdateIsMachineEnvVar() {
  constexpr DWORD kBufferSize = 2;
  StackString<kBufferSize> value;
  const auto length = ::GetEnvironmentVariableW(L"GoogleUpdateIsMachine",
                                                value.get(), kBufferSize);
  return length == 1 && *value.get() == L'1';
}

}  // namespace

Configuration::Configuration() {
  Clear();
}

Configuration::~Configuration() {
  Clear();
}

bool Configuration::Initialize(HMODULE module) {
  Clear();
  return ParseCommandLine(::GetCommandLine());
}

void Configuration::Clear() {
  if (args_) {
    args_.reset();
  }

  command_line_ = nullptr;
  operation_ = INSTALL_PRODUCT;
  argument_count_ = 0;
  is_system_level_ = false;
  has_invalid_switch_ = false;
}

// |command_line| is shared with this instance in the sense that this
// instance may refer to it at will throughout its lifetime, yet it will
// not release it.
bool Configuration::ParseCommandLine(const wchar_t* command_line) {
  command_line_ = command_line;
  args_.reset(::CommandLineToArgvW(command_line_, &argument_count_));
  if (!args_) {
    return false;
  }

  for (int i = 1; i < argument_count_; ++i) {
    if (0 == ::lstrcmpi(args_.get()[i], L"--system-level")) {
      is_system_level_ = true;
    } else if (0 == ::lstrcmpi(args_.get()[i], L"--cleanup")) {
      operation_ = CLEANUP;
    }
  }

  if (!is_system_level_) {
    is_system_level_ = GetGoogleUpdateIsMachineEnvVar();
  }

  return true;
}

}  // namespace updater
