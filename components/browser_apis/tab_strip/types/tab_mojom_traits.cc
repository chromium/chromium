// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/tab_mojom_traits.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"

const tabs_api::NodeId& mojo::StructTraits<MojoTabView, NativeTab>::id(
    const NativeTab& native) {
  return native.id;
}

std::string mojo::StructTraits<MojoTabView, NativeTab>::title(
    const NativeTab& native) {
  return base::UTF16ToUTF8(native.title);
}

const GURL& mojo::StructTraits<MojoTabView, NativeTab>::url(
    const NativeTab& native) {
  return native.url;
}

const gfx::ImageSkia& mojo::StructTraits<MojoTabView, NativeTab>::favicon(
    const NativeTab& native) {
  return native.favicon;
}

const std::vector<tabs::TabAlert>&
mojo::StructTraits<MojoTabView, NativeTab>::alert_states(
    const NativeTab& native) {
  return native.alert_states;
}

tabs::TabNetworkState mojo::StructTraits<MojoTabView, NativeTab>::network_state(
    const NativeTab& native) {
  return native.network_state;
}

bool mojo::StructTraits<MojoTabView, NativeTab>::is_active(
    const NativeTab& native) {
  return native.is_active;
}

bool mojo::StructTraits<MojoTabView, NativeTab>::is_selected(
    const NativeTab& native) {
  return native.is_selected;
}

bool mojo::StructTraits<MojoTabView, NativeTab>::is_blocked(
    const NativeTab& native) {
  return native.is_blocked;
}

base::TimeTicks mojo::StructTraits<MojoTabView, NativeTab>::last_active(
    const NativeTab& native) {
  return native.last_active;
}

bool mojo::StructTraits<MojoTabView, NativeTab>::Read(MojoTabView view,
                                                      NativeTab* out) {
  std::string utf8_title;
  if (!view.ReadId(&out->id) || !view.ReadTitle(&utf8_title) ||
      !view.ReadUrl(&out->url) || !view.ReadFavicon(&out->favicon) ||
      !view.ReadAlertStates(&out->alert_states) ||
      !view.ReadNetworkState(&out->network_state) ||
      !view.ReadLastActive(&out->last_active)) {
    return false;
  }

  out->title = base::UTF8ToUTF16(utf8_title);
  out->is_active = view.is_active();
  out->is_selected = view.is_selected();
  out->is_blocked = view.is_blocked();
  return true;
}
