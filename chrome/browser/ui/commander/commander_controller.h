// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_COMMANDER_CONTROLLER_H_
#define CHROME_BROWSER_UI_COMMANDER_COMMANDER_CONTROLLER_H_

#include "chrome/browser/ui/commander/commander_backend.h"

#include "chrome/browser/ui/commander/command_source.h"

namespace commander {

// The primary CommanderBackend, responsible for aggregating results for
// multiple command sources. If the user selects a composite command (one
// that requires additional user input), this object delegates the
// CommandBackend implementation to |delegate_|.
class CommanderController : public CommanderBackend {
 public:
  using CommandSources = std::vector<std::unique_ptr<CommandSource>>;
  CommanderController();
  ~CommanderController() override;

  // Disallow copy and assign.
  CommanderController(const CommanderController& other) = delete;
  CommanderController& operator=(const CommanderController& other) = delete;

  // CommanderBackend overrides.
  void OnTextChanged(const std::u16string& text, Browser* browser) override;
  void OnCommandSelected(size_t command_index, int result_set_id) override;
  void OnCompositeCommandCancelled() override;
  void SetUpdateCallback(ViewModelUpdateCallback callback) override;
  void Reset() override;

  static std::unique_ptr<CommanderController> CreateWithSourcesForTesting(
      CommandSources sources);

 private:
  explicit CommanderController(CommandSources sources);

  std::vector<std::unique_ptr<CommandItem>> current_items_;
  int current_result_set_id_;
  CommandSources sources_;
  ViewModelUpdateCallback callback_;
  CommandItem::CompositeCommandProvider composite_command_provider_;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_COMMANDER_CONTROLLER_H_
