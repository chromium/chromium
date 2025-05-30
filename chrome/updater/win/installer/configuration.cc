// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/configuration.h"

#include <windows.h>

#include <shellapi.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/scoped_localalloc.h"
#include "chrome/updater/win/installer/string.h"

namespace updater {
namespace {

// Returns true if GoogleUpdateIsMachine=1 is present in the environment.
bool GetGoogleUpdateIsMachineEnvVar() {
  static constexpr DWORD kBufferSize = 2;
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
  operation_ = INSTALL_PRODUCT;
  is_system_level_ = false;
  has_invalid_switch_ = false;
}

// |command_line| is shared with this instance in the sense that this
// instance may refer to it at will throughout its lifetime, yet it will
// not release it.
bool Configuration::ParseCommandLine(const wchar_t* command_line) {
  int argument_count = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> args(
      ::CommandLineToArgvW(command_line, &argument_count));
  if (!args) {
    return false;
  }

  // SAFETY: the unsafe buffer is present due to the ::CommandLineToArgvW call.
  // When constructing the span, `argument_count` is validated and checked as a
  // valid size_t value.
  UNSAFE_BUFFERS(const base::span<wchar_t*> safe_args{
      args.get(), base::checked_cast<size_t>(argument_count)});

  for (size_t i = 1; i < safe_args.size(); ++i) {
    if (0 == ::lstrcmpi(safe_args[i], L"--system-level")) {
      is_system_level_ = true;
    } else if (0 == ::lstrcmpi(safe_args[i], L"--invalid-switch")) {
      has_invalid_switch_ = true;
    } else if (0 == ::lstrcmpi(safe_args[i], L"--cleanup")) {
      operation_ = CLEANUP;
    }
  }

  if (!is_system_level_) {
    is_system_level_ = GetGoogleUpdateIsMachineEnvVar();
  }

  return true;
}

}  // namespace updater
