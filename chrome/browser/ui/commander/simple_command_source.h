// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_SIMPLE_COMMAND_SOURCE_H_
#define CHROME_BROWSER_UI_COMMANDER_SIMPLE_COMMAND_SOURCE_H_

#include "chrome/browser/ui/commander/command_source.h"

#include "base/memory/weak_ptr.h"

namespace commander {

// A command source which hosts simple one-shot browser commands, most of which
// are accessible by hotkey. This is an alternative surface to provide for
// hotkey discovery.
class SimpleCommandSource : public CommandSource {
 public:
  SimpleCommandSource();
  ~SimpleCommandSource() override;

  // Disallow copy and assign
  SimpleCommandSource(const SimpleCommandSource& other) = delete;
  SimpleCommandSource& operator=(const SimpleCommandSource& other) = delete;

  // Command source overrides
  CommandSource::CommandResults GetCommands(const std::u16string& input,
                                            Browser* browser) const override;

 private:
  base::WeakPtr<SimpleCommandSource> weak_this_;
  // Wrapper around chrome::ExecuteCommand. See implementation comment
  // for details.
  void ExecuteCommand(Browser* browser, int command_id);
  base::WeakPtrFactory<SimpleCommandSource> weak_ptr_factory_{this};
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_SIMPLE_COMMAND_SOURCE_H_
