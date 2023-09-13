// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/process_singleton_dialog_linux.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

bool ShowProcessSingletonDialog(const std::u16string& message,
                                const std::u16string& relaunch_text) {
  bool result = chrome::ShowMessageBoxWithButtonText(
                    nullptr, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                    message, relaunch_text,
                    l10n_util::GetStringUTF16(IDS_PROFILE_IN_USE_LINUX_QUIT)) ==
                chrome::MESSAGE_BOX_RESULT_YES;

  // Avoids gpu_process_transport_factory.cc(153)] Check failed:
  // per_compositor_data_.empty() when quit is chosen.
  base::RunLoop().RunUntilIdle();

  return result;
}
