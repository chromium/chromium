// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wm/coral_delegate_impl.h"

CoralDelegateImpl::CoralDelegateImpl() = default;

CoralDelegateImpl::~CoralDelegateImpl() = default;

void CoralDelegateImpl::LaunchPostLoginCluster(
    const ash::coral_util::CoralCluster& cluster) {}

void CoralDelegateImpl::OpenNewDeskWithCluster(
    const ash::coral_util::CoralCluster& cluster) {}

void CoralDelegateImpl::CreateSavedDeskFromCluster(
    const ash::coral_util::CoralCluster& cluster) {}
