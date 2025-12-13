// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/position.h"

namespace tabs_api {

Position::Position() : index_(0) {}
Position::Position(size_t index) : index_(index) {}
Position::Position(size_t index, std::optional<tabs_api::NodeId> parent_id)
    : index_(index), parent_id_(std::move(parent_id)) {}
Position::~Position() = default;

Position::Position(const Position& other) = default;
Position::Position(Position&& other) noexcept = default;
Position& Position::operator=(const Position& other) = default;
Position& Position::operator=(Position&& other) noexcept = default;

bool operator==(const Position& a, const Position& b) {
  return a.index() == b.index() && a.parent_id() == b.parent_id();
}

}  // namespace tabs_api
