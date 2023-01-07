// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_COMMAND_SOURCE_H_
#define CHROME_BROWSER_UI_COMMANDER_COMMAND_SOURCE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/range/range.h"

class Browser;

namespace commander {

struct CommandItem;

// Provides and ranks available commands in response to user input. The
// intention is for every system available through the commander to
// provide its own source, which is responsible for tracking the state and
// context necessary to provide appropriate commands from that system.
class CommandSource {
 public:
  using CommandResults = std::vector<std::unique_ptr<CommandItem>>;
  CommandSource() = default;
  virtual ~CommandSource() = default;

  // Returns a list of scored commands to return for |input|, or an empty
  // list if none are appropriate. The commands are not guaranteed to be in
  // any particular order. |browser| is the browser the active commander
  // is attached to.
  virtual CommandResults GetCommands(const std::u16string& input,
                                     Browser* browser) const = 0;
};

// Represents a single option that can be presented in the command palette.
struct CommandItem {
 public:
  enum Type {
    // On selection, the command is invoked and the UI should close.
    kOneShot,
    // On selection, the user is prompted for further information.
    kComposite,
  };
  // What *the text* of this command represents. For example, in the composite
  // command "Move Current Tab To Window", the user will be prompted to select
  // a window by name. In that case, the original command will have Entity =
  // kCommand, and the follow-up will have Entity kWindow.
  // This is used in the UI to give different visual treatments to different
  // entity types.
  enum Entity {
    kCommand,
    kBookmark,
    kTab,
    kWindow,
    kGroup,
  };

  using CompositeCommandProvider =
      base::RepeatingCallback<CommandSource::CommandResults(
          const std::u16string&)>;
  using CompositeCommand = std::pair<std::u16string, CompositeCommandProvider>;

  CommandItem();
  CommandItem(const std::u16string& title,
              double score,
              const std::vector<gfx::Range>& ranges);
  virtual ~CommandItem();

  CommandItem(const CommandItem& other) = delete;
  CommandItem& operator=(const CommandItem& other) = delete;
  CommandItem(CommandItem&& other);
  CommandItem& operator=(CommandItem&& other);

  Type GetType();
  // The title to display to the user.
  std::u16string title;
  // See Entity documentation above.
  Entity entity_type = kCommand;
  // Optional secondary text for the command. Typically used to display a
  // hotkey.
  std::u16string annotation;
  // If this command is a one-shot, executes the command. If this command is
  // composite, provides the prompt text sent to the user, and a
  // CompositeCommandProvider to handle additional user input.
  absl::variant<base::OnceClosure, CompositeCommand> command;
  // How relevant the item is to user input. Expected range is (0,1], with 1
  // indicating a perfect match (in the absence of other criteria, this boils
  // down to an exact string match).
  double score;
  // Ranges of indices in |item|'s title that correspond to user input.
  // For example, given user input "comitmlt" and a command called "Command Item
  // Match Result", this would result in {(0, 3), (8, 10), (13,14), (23,25)},
  // representing:
  //    [Com]mand [It]em [M]atch Resu[lt]
  std::vector<gfx::Range> matched_ranges;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_COMMAND_SOURCE_H_
