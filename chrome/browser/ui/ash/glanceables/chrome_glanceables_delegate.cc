// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/chrome_glanceables_delegate.h"

#include "ash/glanceables/glanceables_controller.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/app_restore/full_restore_save_handler.h"

namespace {
ChromeGlanceablesDelegate* g_instance = nullptr;
}  // namespace

ChromeGlanceablesDelegate::ChromeGlanceablesDelegate(
    ash::GlanceablesController* controller)
    : controller_(controller) {
  DCHECK(controller_);

  DCHECK(!g_instance);
  g_instance = this;
}

ChromeGlanceablesDelegate::~ChromeGlanceablesDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ChromeGlanceablesDelegate* ChromeGlanceablesDelegate::Get() {
  return g_instance;
}

void ChromeGlanceablesDelegate::OnPrimaryUserSessionStarted(Profile* profile) {
  primary_profile_ = profile;
  if (ShouldShowOnLogin())
    controller_->ShowOnLogin();
}

void ChromeGlanceablesDelegate::RestoreSession() {
  if (!primary_profile_ || did_restore_)
    return;
  auto* full_restore_service =
      ash::full_restore::FullRestoreService::GetForProfile(primary_profile_);
  if (!full_restore_service)
    return;
  full_restore_service->Restore();
  did_restore_ = true;
}

void ChromeGlanceablesDelegate::OnGlanceablesClosed() {
  if (!did_restore_) {
    // The user closed glanceables without triggering a session restore, so
    // start the full restore state save timer.
    ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
  }
}

bool ChromeGlanceablesDelegate::ShouldShowOnLogin() const {
  // Skip glanceables when --no-first-run is passed. This prevents glanceables
  // from interfering with existing browser tests (they pass this switch) and is
  // also helpful when bisecting.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoFirstRun))
    return false;

  // Don't show glanceables for session types that don't support full restore
  // (e.g. demo mode, forced app mode).
  // TODO(crbug.com/1353119): Refine triggering logic based on PM/UX feedback.
  if (!ash::full_restore::FullRestoreServiceFactory::
          IsFullRestoreAvailableForProfile(primary_profile_)) {
    return false;
  }

  return true;
}
