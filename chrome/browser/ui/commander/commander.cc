// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/commander.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/commander/commander_controller.h"
#include "chrome/browser/ui/commander/commander_frontend.h"
#include "chrome/browser/ui/ui_features.h"
#include "content/public/browser/browser_thread.h"

namespace commander {

bool IsEnabled() {
  return base::FeatureList::IsEnabled(features::kQuickCommands);
}

// static
Commander* Commander::Get() {
  static base::NoDestructor<Commander> instance;
  return instance.get();
}

Commander::Commander() = default;
Commander::~Commander() = default;

void Commander::Initialize() {
  DCHECK(IsEnabled());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!backend_ && !frontend_);
  backend_ = std::make_unique<CommanderController>();
  frontend_ = CommanderFrontend::Create(backend_.get());
}

void Commander::ToggleForBrowser(Browser* browser) {
  DCHECK(IsEnabled());
  DCHECK(frontend_);
  frontend_->ToggleForBrowser(browser);
}

}  // namespace commander
