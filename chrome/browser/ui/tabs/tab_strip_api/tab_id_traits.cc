// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_id_traits.h"

MojoTabType mojo::EnumTraits<MojoTabType, NativeTabType>::ToMojom(
    NativeTabType input) {
  switch (input) {
    case NativeTabType::kContent:
      return MojoTabType::kContent;
    case NativeTabType::kCollection:
      return MojoTabType::kCollection;
    case NativeTabType::kInvalid:
      return MojoTabType::kUnknown;
  }

  NOTREACHED();
}

bool mojo::EnumTraits<MojoTabType, NativeTabType>::FromMojom(
    MojoTabType in,
    NativeTabType* out) {
  switch (in) {
    case MojoTabType::kContent:
      *out = NativeTabType::kContent;
      return true;
    case MojoTabType::kCollection:
      *out = NativeTabType::kCollection;
      return true;
    case MojoTabType::kUnknown:
      *out = NativeTabType::kInvalid;
      return true;
  }

  NOTREACHED();
}

std::string_view mojo::StructTraits<MojoTabIdView, NativeTabId>::id(
    const NativeTabId& native) {
  return native.Id();
}

NativeTabType mojo::StructTraits<MojoTabIdView, NativeTabId>::type(
    const NativeTabId& native) {
  return native.Type();
}

bool mojo::StructTraits<MojoTabIdView, NativeTabId>::Read(MojoTabIdView view,
                                                          NativeTabId* out) {
  std::string id;
  if (!view.ReadId(&id)) {
    return false;
  }

  NativeTabType type;
  if (!view.ReadType(&type)) {
    return false;
  }
  *out = NativeTabId(type, id);
  return true;
}
