// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/commander_controller.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/commander/command_source.h"
#include "chrome/browser/ui/commander/commander_view_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"

namespace commander {

namespace {

class TestCommandSource : public CommandSource {
 public:
  using GetCommandsHandler =
      base::RepeatingCallback<CommandResults(const base::string16&,
                                             Browser* browser)>;
  explicit TestCommandSource(GetCommandsHandler handler)
      : handler_(std::move(handler)) {}
  ~TestCommandSource() override = default;

  CommandResults GetCommands(const base::string16& input,
                             Browser* browser) const override {
    invocations_.push_back(input);
    return handler_.Run(input, browser);
  }

  const std::vector<base::string16>& invocations() const {
    return invocations_;
  }

 private:
  mutable std::vector<base::string16> invocations_;
  GetCommandsHandler handler_;
};

std::unique_ptr<TestCommandSource> CreateNoOpCommandSource() {
  return std::make_unique<TestCommandSource>(base::BindRepeating(
      [](const base::string16&,
         Browser* browser) -> CommandSource::CommandResults { return {}; }));
}

std::unique_ptr<CommandItem> CreateNoOpCommandItem(const base::string16& title,
                                                   double score) {
  auto item = std::make_unique<CommandItem>();
  item->title = title;
  item->score = score;
  item->matched_ranges.emplace_back(0, title.size());
  item->command = base::BindOnce([]() {});
  return item;
}

template <typename T>
std::unique_ptr<CommandItem> CreateCompositeCommandItem(
    const base::string16& title,
    double scope) {}

TestCommandSource* AddSource(
    std::vector<std::unique_ptr<CommandSource>>* sources,
    std::unique_ptr<TestCommandSource> source) {
  TestCommandSource* bare_ptr = source.get();
  sources->push_back(std::move(source));
  return bare_ptr;
}

}  // namespace

class CommanderControllerTest : public BrowserWithTestWindowTest {
 public:
  class TestBackend : public CommanderBackend {
   public:
    explicit TestBackend(CommanderControllerTest* owner) {
      owner->SetTestBackend(this);
    }
    void OnTextChanged(const base::string16& text, Browser* browser) override {
      text_changed_invocations_.push_back(text);
    }
    void OnCommandSelected(size_t command_index, int result_set_id) override {
      command_selected_invocations_.push_back(command_index);
    }
    void SetUpdateCallback(ViewModelUpdateCallback callback) override {
      callback_ = std::move(callback);
    }

    void CallCallback() {
      CommanderViewModel vm;
      callback_.Run(vm);
    }
    const std::vector<base::string16> text_changed_invocations() {
      return text_changed_invocations_;
    }
    const std::vector<size_t> command_selected_invocations() {
      return command_selected_invocations_;
    }

   private:
    ViewModelUpdateCallback callback_;
    std::vector<base::string16> text_changed_invocations_;
    std::vector<size_t> command_selected_invocations_;
  };

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    expected_count_ = 0;
  }

  void ExpectViewModelCallbackCalls(int expected_count) {
    expected_count_ += expected_count;
  }
  void WaitForExpectedCallbacks() {
    if (expected_count_ <= 0)
      return;
    if (!run_loop_.get() || !run_loop_->running()) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }
  void OnViewModelUpdated(CommanderViewModel view_model) {
    received_view_models_.push_back(view_model);
    if (expected_count_ > 0) {
      expected_count_--;
      if (run_loop_.get() && expected_count_ == 0)
        run_loop_->Quit();
    }
  }

  void SetTestBackend(TestBackend* test_backend) {
    test_backend_ = test_backend;
  }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
  int expected_count_;
  std::vector<CommanderViewModel> received_view_models_;
  TestBackend* test_backend_;
};

class ViewModelCallbackWaiter {
 public:
  explicit ViewModelCallbackWaiter(CommanderControllerTest* test, int count = 1)
      : test_(test) {
    test_->ExpectViewModelCallbackCalls(count);
  }

  ~ViewModelCallbackWaiter() { test_->WaitForExpectedCallbacks(); }

 private:
  CommanderControllerTest* test_;
};

TEST_F(CommanderControllerTest, PassesInputToCommandSourcesOnTextChanged) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  TestCommandSource* first = AddSource(&sources, CreateNoOpCommandSource());
  TestCommandSource* second = AddSource(&sources, CreateNoOpCommandSource());

  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  EXPECT_EQ(first->invocations().size(), 0u);
  EXPECT_EQ(second->invocations().size(), 0u);

  base::string16 input = base::ASCIIToUTF16("foobar");
  controller->OnTextChanged(input, browser());

  EXPECT_EQ(first->invocations().size(), 1u);
  EXPECT_EQ(second->invocations().size(), 1u);

  EXPECT_EQ(first->invocations().back(), input);
  EXPECT_EQ(second->invocations().back(), input);
}

TEST_F(CommanderControllerTest, ResultSetIdsDifferAcrossCalls) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  ignore_result(AddSource(&sources, CreateNoOpCommandSource()));
  base::RunLoop run_loop;
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("foobar"), browser());
  }
  // Assert since we're accessing an element.
  ASSERT_EQ(received_view_models_.size(), 1u);
  int first_id = received_view_models_.back().result_set_id;

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("barfoo"), browser());
  }
  EXPECT_EQ(received_view_models_.size(), 2u);
  EXPECT_NE(received_view_models_.back().result_set_id, first_id);
}

TEST_F(CommanderControllerTest, ViewModelAggregatesResults) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto first = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const base::string16&, Browser* browser) {
        CommandSource::CommandResults result;
        result.push_back(
            CreateNoOpCommandItem(base::ASCIIToUTF16("first"), 100));
        return result;
      }));
  auto second = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const base::string16&, Browser* browser) {
        CommandSource::CommandResults result;
        auto item = CreateNoOpCommandItem(base::ASCIIToUTF16("second"), 99);
        item->annotation = base::ASCIIToUTF16("2nd");
        item->entity_type = CommandItem::Entity::kBookmark;
        result.push_back(std::move(item));
        return result;
      }));
  sources.push_back(std::move(first));
  sources.push_back(std::move(second));

  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("foobar"), browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  ASSERT_EQ(model.items.size(), 2u);

  EXPECT_EQ(model.items[0].title, base::ASCIIToUTF16("first"));
  EXPECT_EQ(model.items[0].annotation, base::string16());
  EXPECT_EQ(model.items[0].entity_type, CommandItem::Entity::kCommand);

  EXPECT_EQ(model.items[1].title, base::ASCIIToUTF16("second"));
  EXPECT_EQ(model.items[1].annotation, base::ASCIIToUTF16("2nd"));
  EXPECT_EQ(model.items[1].entity_type, CommandItem::Entity::kBookmark);
}

// TODO(lgrey): This will need to change when scoring gets more sophisticated
// than a simple sort.
TEST_F(CommanderControllerTest, ViewModelSortsResults) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto first = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const base::string16&, Browser* browser) {
        CommandSource::CommandResults result;
        result.push_back(
            CreateNoOpCommandItem(base::ASCIIToUTF16("third"), 98));
        result.push_back(
            CreateNoOpCommandItem(base::ASCIIToUTF16("first"), 100));
        result.push_back(
            CreateNoOpCommandItem(base::ASCIIToUTF16("fourth"), 90));

        return result;
      }));
  auto second = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const base::string16&, Browser* browser) {
        CommandSource::CommandResults result;
        result.push_back(
            CreateNoOpCommandItem(base::ASCIIToUTF16("second"), 99));
        result.push_back(CreateNoOpCommandItem(base::ASCIIToUTF16("fifth"), 1));
        return result;
      }));
  sources.push_back(std::move(first));
  sources.push_back(std::move(second));

  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("foobar"), browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  ASSERT_EQ(model.items.size(), 5u);
  EXPECT_EQ(model.items[0].title, base::ASCIIToUTF16("first"));
  EXPECT_EQ(model.items[1].title, base::ASCIIToUTF16("second"));
  EXPECT_EQ(model.items[2].title, base::ASCIIToUTF16("third"));
  EXPECT_EQ(model.items[3].title, base::ASCIIToUTF16("fourth"));
  EXPECT_EQ(model.items[4].title, base::ASCIIToUTF16("fifth"));
}

TEST_F(CommanderControllerTest, ViewModelRetainsBoldRanges) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto source = std::make_unique<TestCommandSource>(
      base::BindRepeating([=](const base::string16&, Browser* browser) {
        auto first = CreateNoOpCommandItem(base::ASCIIToUTF16("first"), 100);
        auto second = CreateNoOpCommandItem(base::ASCIIToUTF16("second"), 99);
        first->matched_ranges.clear();
        first->matched_ranges.emplace_back(0, 2);
        first->matched_ranges.emplace_back(4, 1);
        second->matched_ranges.clear();
        second->matched_ranges.emplace_back(1, 4);
        CommandSource::CommandResults result;
        result.push_back(std::move(first));
        result.push_back(std::move(second));
        return result;
      }));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("foobar"), browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  // Ensure |first| is at index 0;
  EXPECT_EQ(model.items[0].title, base::ASCIIToUTF16("first"));
  std::vector<gfx::Range> first_ranges = {gfx::Range(0, 2), gfx::Range(4, 1)};
  std::vector<gfx::Range> second_ranges = {gfx::Range(1, 4)};
  EXPECT_EQ(model.items[0].matched_ranges, first_ranges);
  EXPECT_EQ(model.items[1].matched_ranges, second_ranges);
}

TEST_F(CommanderControllerTest, OnCommandSelectedInvokesOneShotCommand) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  bool first_called = false;
  bool second_called = false;
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](bool* first_called_ptr, bool* second_called_ptr, const base::string16&,
         Browser* browser) {
        auto first = CreateNoOpCommandItem(base::ASCIIToUTF16("first"), 100);
        auto second = CreateNoOpCommandItem(base::ASCIIToUTF16("second"), 99);
        first->command = base::BindOnce([](bool* called) { *called = true; },
                                        first_called_ptr);
        second->command = base::BindOnce([](bool* called) { *called = true; },
                                         second_called_ptr);
        CommandSource::CommandResults result;
        result.push_back(std::move(first));
        result.push_back(std::move(second));
        return result;
      },
      &first_called, &second_called));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("foobar"), browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  // Ensure |first| is at index 0;
  EXPECT_EQ(model.items[0].title, base::ASCIIToUTF16("first"));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnCommandSelected(0, model.result_set_id);
  }
  EXPECT_TRUE(first_called);
  EXPECT_FALSE(second_called);
  EXPECT_EQ(received_view_models_.size(), 2u);
  EXPECT_EQ(received_view_models_.back().action,
            CommanderViewModel::Action::kClose);
}

TEST_F(CommanderControllerTest, NoActionOnIncorrectResultId) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  bool item_called = false;
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](bool* called_ptr, const base::string16&, Browser* browser) {
        auto item = CreateNoOpCommandItem(base::ASCIIToUTF16("first"), 100);
        item->command =
            base::BindOnce([](bool* called) { *called = true; }, called_ptr);
        CommandSource::CommandResults result;
        result.push_back(std::move(item));
        return result;
      },
      &item_called));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("foobar"), browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();

  controller->OnCommandSelected(0, model.result_set_id - 1);

  EXPECT_FALSE(item_called);
}

TEST_F(CommanderControllerTest, NoActionOnOOBIndex) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  bool item_called = false;
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](bool* called_ptr, const base::string16&, Browser* browser) {
        auto item = CreateNoOpCommandItem(base::ASCIIToUTF16("first"), 100);
        item->command =
            base::BindOnce([](bool* called) { *called = true; }, called_ptr);
        CommandSource::CommandResults result;
        result.push_back(std::move(item));
        return result;
      },
      &item_called));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("foobar"), browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();

  controller->OnCommandSelected(1, model.result_set_id);

  EXPECT_FALSE(item_called);
}

TEST_F(CommanderControllerTest, InvokingCompositeCommandSendsPrompt) {
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](CommanderControllerTest* instance, const base::string16&,
         Browser* browser) -> CommandSource::CommandResults {
        auto item = std::make_unique<CommandItem>();
        item->title = base::ASCIIToUTF16("Do something...");
        item->score = 100;
        item->matched_ranges.emplace_back(0, item->title.size());
        item->delegate_factory = base::BindOnce(
            [](CommanderControllerTest* instance)
                -> std::unique_ptr<CommanderBackend> {
              return std::make_unique<TestBackend>(instance);
            },
            instance);
        CommandSource::CommandResults results;
        results.push_back(std::move(item));
        return results;
      },
      this));
  std::vector<std::unique_ptr<CommandSource>> sources;
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("abracadabra"), browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  {
    ViewModelCallbackWaiter waiter(this);

    controller->OnCommandSelected(0,
                                  received_view_models_.back().result_set_id);
  }
  EXPECT_EQ(received_view_models_.back().action,
            CommanderViewModel::Action::kPrompt);
}

TEST_F(CommanderControllerTest, OnTextChangedPassedToDelegate) {
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](CommanderControllerTest* instance, const base::string16&,
         Browser* browser) -> CommandSource::CommandResults {
        auto item = std::make_unique<CommandItem>();
        item->title = base::ASCIIToUTF16("Do something...");
        item->score = 100;
        item->matched_ranges.emplace_back(0, item->title.size());
        item->delegate_factory = base::BindOnce(
            [](CommanderControllerTest* instance)
                -> std::unique_ptr<CommanderBackend> {
              return std::make_unique<TestBackend>(instance);
            },
            instance);
        CommandSource::CommandResults results;
        results.push_back(std::move(item));
        return results;
      },
      this));
  std::vector<std::unique_ptr<CommandSource>> sources;
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("abracadabra"), browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  {
    ViewModelCallbackWaiter waiter(this);

    controller->OnCommandSelected(0,
                                  received_view_models_.back().result_set_id);
  }
  EXPECT_TRUE(test_backend_->text_changed_invocations().empty());

  controller->OnTextChanged(base::ASCIIToUTF16("hocus pocus"), browser());
  ASSERT_EQ(test_backend_->text_changed_invocations().size(), 1u);
  EXPECT_EQ(test_backend_->text_changed_invocations().back(),
            base::ASCIIToUTF16("hocus pocus"));
}

TEST_F(CommanderControllerTest, OnCommandSelectedPassedToDelegate) {
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](CommanderControllerTest* instance, const base::string16&,
         Browser* browser) -> CommandSource::CommandResults {
        auto item = std::make_unique<CommandItem>();
        item->title = base::ASCIIToUTF16("Do something...");
        item->score = 100;
        item->matched_ranges.emplace_back(0, item->title.size());
        item->delegate_factory = base::BindOnce(
            [](CommanderControllerTest* instance)
                -> std::unique_ptr<CommanderBackend> {
              return std::make_unique<TestBackend>(instance);
            },
            instance);
        CommandSource::CommandResults results;
        results.push_back(std::move(item));
        return results;
      },
      this));
  std::vector<std::unique_ptr<CommandSource>> sources;
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(base::ASCIIToUTF16("abracadabra"), browser());
  }

  ASSERT_EQ(received_view_models_.size(), 1u);

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnCommandSelected(0,
                                  received_view_models_.back().result_set_id);
  }

  EXPECT_TRUE(test_backend_->text_changed_invocations().empty());

  controller->OnCommandSelected(586,
                                received_view_models_.back().result_set_id);
  ASSERT_EQ(test_backend_->command_selected_invocations().size(), 1u);
  EXPECT_EQ(test_backend_->command_selected_invocations().back(), 586u);
}

}  // namespace commander
