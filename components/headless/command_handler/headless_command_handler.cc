// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/command_handler/headless_command_handler.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/headless/command_handler/grit/headless_command_resources.h"
#include "components/headless/command_handler/headless_command_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

namespace {

// Default file name for screenshot. Can be overridden by "--screenshot" switch.
const char kDefaultScreenshotFileName[] = "screenshot.png";
// Default file name for pdf. Can be overridden by "--print-to-pdf" switch.
const char kDefaultPDFFileName[] = "output.pdf";

const char kChromeHeadlessHost[] = "headless";
const char kChromeHeadlessURL[] = "chrome://headless/";

const char kHeadlessCommandHtml[] = "headless_command.html";
const char kHeadlessCommandJs[] = "headless_command.js";

// Specifies the initial window size: --window-size=w,h. Headless Chrome users
// historically use this to specify expected screenshot size. Originally defined
// in //chrome/common/chrome_switches.h which we cannot include from here.
const char kWindowSize[] = "window-size";

HeadlessCommandHandler::DoneCallback& GetGlobalDoneCallback() {
  static base::NoDestructor<HeadlessCommandHandler::DoneCallback> done_callback;
  return *done_callback;
}

void EnsureHeadlessCommandResources() {
  // Check if our resources are already loaded and bail out early. This happens
  // when running Chrome with --headless switch and headless command resources
  // have been merged into the Chrome's resource pack.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  if (!bundle.GetRawDataResource(IDR_HEADLESS_COMMAND_HTML).empty()) {
    DCHECK(!bundle.GetRawDataResource(IDR_HEADLESS_COMMAND_JS).empty());
    return;
  }

  // Check if we have headless command resource pack next to our binary and load
  // it if so. Note that this code is expected to run early during the browser
  // startup phase when file system access is still allowed.
  base::FilePath resource_dir;
  bool result = base::PathService::Get(base::DIR_ASSETS, &resource_dir);
  DCHECK(result);

  base::FilePath resource_pack =
      resource_dir.Append(FILE_PATH_LITERAL("headless_command_resources.pak"));
  if (base::PathExists(resource_pack)) {
    bundle.AddDataPackFromPath(resource_pack, ui::kScaleFactorNone);
  }
}

void CreateAndAddHeadlessHostDataSource(
    content::BrowserContext* browser_context) {
  EnsureHeadlessCommandResources();

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeHeadlessHost);
  DCHECK(source);

  source->AddResourcePath(kHeadlessCommandHtml, IDR_HEADLESS_COMMAND_HTML);
  source->AddResourcePath(kHeadlessCommandJs, IDR_HEADLESS_COMMAND_JS);
}

base::Value::Dict GetColorDictFromHexColor(uint32_t color, bool has_alpha) {
  base::Value::Dict dict;
  if (has_alpha) {
    dict.Set("r", static_cast<int>((color & 0xff000000) >> 24));
    dict.Set("g", static_cast<int>((color & 0x00ff0000) >> 16));
    dict.Set("b", static_cast<int>((color & 0x0000ff00) >> 8));
    dict.Set("a", static_cast<int>((color & 0x000000ff)));
  } else {
    dict.Set("r", static_cast<int>((color & 0xff0000) >> 16));
    dict.Set("g", static_cast<int>((color & 0x00ff00) >> 8));
    dict.Set("b", static_cast<int>((color & 0x0000ff)));
  }

  return dict;
}

bool ParseWindowSize(const std::string& window_size, int* width, int* height) {
  std::vector<std::string_view> width_and_height = base::SplitStringPiece(
      window_size, ",x", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (width_and_height.size() != 2 ||
      !base::StringToInt(width_and_height[0], width) ||
      !base::StringToInt(width_and_height[1], height)) {
    return false;
  }

  return *width > 0 && *height > 0;
}

bool GetCommandDictAndOutputPaths(base::Value::Dict* commands,
                                  base::FilePath* pdf_file_path,
                                  base::FilePath* screenshot_file_path) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // --dump-dom
  if (command_line->HasSwitch(switches::kDumpDom)) {
    commands->Set("dumpDom", true);
  }

  // --print-to-pdf=[output path]
  if (command_line->HasSwitch(switches::kPrintToPDF)) {
    base::FilePath path =
        command_line->GetSwitchValuePath(switches::kPrintToPDF);
    if (path.empty()) {
      path = base::FilePath().AppendASCII(kDefaultPDFFileName);
    }
    *pdf_file_path = path;

    base::Value::Dict params;
    if (command_line->HasSwitch(switches::kNoPDFHeaderFooter)) {
      params.Set("noHeaderFooter", true);
    }

    if (command_line->HasSwitch(switches::kDisablePDFTagging)) {
      params.Set("disablePDFTagging", true);
    }

    if (command_line->HasSwitch(switches::kGeneratePDFDocumentOutline)) {
      params.Set("generateDocumentOutline", true);
    }

    commands->Set("printToPDF", std::move(params));
  }

  // --screenshot=[output path]
  if (command_line->HasSwitch(switches::kScreenshot)) {
    base::FilePath path =
        command_line->GetSwitchValuePath(switches::kScreenshot);
    if (path.empty()) {
      path = base::FilePath().AppendASCII(kDefaultScreenshotFileName);
    }
    *screenshot_file_path = path;

    base::FilePath::StringType extension =
        base::ToLowerASCII(path.FinalExtension());

    static constexpr auto kImageFileTypes =
        base::MakeFixedFlatMap<base::FilePath::StringPieceType, const char*>({
            {FILE_PATH_LITERAL(".jpeg"), "jpeg"},
            {FILE_PATH_LITERAL(".jpg"), "jpeg"},
            {FILE_PATH_LITERAL(".png"), "png"},
            {FILE_PATH_LITERAL(".webp"), "webp"},
        });

    auto it = kImageFileTypes.find(extension);
    if (it == kImageFileTypes.cend()) {
      LOG(ERROR) << "Unsupported screenshot image file type: "
                 << path.FinalExtension();
      return false;
    }

    base::Value::Dict params;
    params.Set("format", it->second);

    if (command_line->HasSwitch(kWindowSize)) {
      int width, height;
      if (ParseWindowSize(command_line->GetSwitchValueASCII(kWindowSize),
                          &width, &height)) {
        params.Set("width", width);
        params.Set("height", height);
      } else {
        LOG(ERROR) << "Invalid --" << kWindowSize << " specification ignored";
      }
    }

    commands->Set("screenshot", std::move(params));
  }

  // --default-background-color=rrggbb[aa]
  if (command_line->HasSwitch(switches::kDefaultBackgroundColor)) {
    std::string hex_color =
        command_line->GetSwitchValueASCII(switches::kDefaultBackgroundColor);
    uint32_t color;
    if (!(hex_color.length() == 6 || hex_color.length() == 8) ||
        !base::HexStringToUInt(hex_color, &color)) {
      LOG(ERROR)
          << "Expected a hex RGB or RGBA value for --default-background-color="
          << hex_color;
      return false;
    }

    commands->Set("defaultBackgroundColor",
                  GetColorDictFromHexColor(color, hex_color.length() == 8));
  }

  // virtual-time-budget=[ms]
  if (command_line->HasSwitch(switches::kVirtualTimeBudget)) {
    std::string budget_ms_str =
        command_line->GetSwitchValueASCII(switches::kVirtualTimeBudget);
    int budget_ms;
    if (!base::StringToInt(budget_ms_str, &budget_ms)) {
      LOG(ERROR) << "Expected an integer value for --virtual-time-budget="
                 << budget_ms_str;
      return false;
    }

    commands->Set("virtualTimeBudget", budget_ms);
  }

  // timeout=[ms]
  if (command_line->HasSwitch(switches::kTimeout)) {
    std::string timeout_ms_str =
        command_line->GetSwitchValueASCII(switches::kTimeout);
    int timeout_ms;
    if (!base::StringToInt(timeout_ms_str, &timeout_ms)) {
      LOG(ERROR) << "Expected an integer value for --timeout="
                 << timeout_ms_str;
      return false;
    }

    commands->Set("timeout", timeout_ms);
  }

  return true;
}

bool WriteFileTask(base::FilePath file_path, std::string file_data) {
  auto file_span = base::as_byte_span(file_data);
  if (!base::WriteFile(file_path, file_span)) {
    PLOG(ERROR) << "Failed to write file " << file_path;
    return false;
  }

  std::cerr << file_data.size() << " bytes written to file " << file_path
            << std::endl;
  return true;
}

}  // namespace

HeadlessCommandHandler::HeadlessCommandHandler(
    content::WebContents* web_contents,
    GURL target_url,
    DoneCallback done_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : target_url_(std::move(target_url)),
      done_callback_(std::move(done_callback)),
      io_task_runner_(std::move(io_task_runner)) {
  DCHECK(web_contents);
  DCHECK(io_task_runner_);

  content::WebContentsObserver::Observe(web_contents);

  // Load command execution harness resources and create URL data source
  // for chrome://headless.
  CreateAndAddHeadlessHostDataSource(web_contents->GetBrowserContext());

  browser_devtools_client_.AttachToBrowser();
  devtools_client_.AttachToWebContents(web_contents);
}

HeadlessCommandHandler::~HeadlessCommandHandler() = default;

// static
GURL HeadlessCommandHandler::GetHandlerUrl() {
  const std::string url =
      base::StrCat({kChromeHeadlessURL, kHeadlessCommandHtml});
  return GURL(url);
}

// static
bool HeadlessCommandHandler::HasHeadlessCommandSwitches(
    const base::CommandLine& command_line) {
  static const char* kCommandSwitches[] = {
      switches::kDefaultBackgroundColor,
      switches::kDumpDom,
      switches::kPrintToPDF,
      switches::kNoPDFHeaderFooter,
      switches::kScreenshot,
      switches::kTimeout,
      switches::kVirtualTimeBudget,
  };

  for (const char* command_switch : kCommandSwitches) {
    if (command_line.HasSwitch(command_switch)) {
      return true;
    }
  }

  return false;
}

// static
void HeadlessCommandHandler::ProcessCommands(
    content::WebContents* web_contents,
    GURL target_url,
    DoneCallback done_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  if (!io_task_runner) {
    io_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  // Headless Command Handler instance will self delete when done.
  new HeadlessCommandHandler(web_contents, std::move(target_url),
                             std::move(done_callback), io_task_runner);
}

void HeadlessCommandHandler::DocumentOnLoadCompletedInPrimaryMainFrame() {
  base::Value::Dict commands;
  if (!GetCommandDictAndOutputPaths(&commands, &pdf_file_path_,
                                    &screenshot_file_path_) ||
      commands.empty()) {
    PostDone();
    return;
  }

  commands.Set("targetUrl", target_url_.spec());

  // Expose DevTools protocol to the target.
  base::Value::Dict expose_params;
  expose_params.Set("targetId", devtools_client_.GetTargetId());
  browser_devtools_client_.SendCommand("Target.exposeDevToolsProtocol",
                                       std::move(expose_params));

  // Set up Inspector domain.
  devtools_client_.AddEventHandler(
      "Inspector.targetCrashed",
      base::BindRepeating(&HeadlessCommandHandler::OnTargetCrashed,
                          base::Unretained(this)));
  devtools_client_.SendCommand("Inspector.enable");

  std::string json_commands;
  base::JSONWriter::Write(commands, &json_commands);
  std::string script = "executeCommands(JSON.parse('" + json_commands + "'))";

  base::Value::Dict params;
  params.Set("expression", script);
  params.Set("awaitPromise", true);
  params.Set("returnByValue", true);
  devtools_client_.SendCommand(
      "Runtime.evaluate", std::move(params),
      base::BindOnce(&HeadlessCommandHandler::OnCommandsResult,
                     base::Unretained(this)));
}

void HeadlessCommandHandler::WebContentsDestroyed() {
  LOG(ERROR) << "Unexpected renderer destruction.";
  Done();
}

void HeadlessCommandHandler::OnTargetCrashed(const base::Value::Dict&) {
  LOG(ERROR) << "Abnormal renderer termination.";
  Done();
}

void HeadlessCommandHandler::OnCommandsResult(base::Value::Dict result) {
  if (result.FindBoolByDottedPath("result.result.value.pageLoadTimedOut")
          .value_or(false)) {
    result_ = Result::kPageLoadTimeout;
    LOG(ERROR) << "Page load timed out.";
  }

  if (std::string* dom_dump =
          result.FindStringByDottedPath("result.result.value.dumpDomResult")) {
    std::cout << *dom_dump << std::endl;
  }

  if (std::string* base64_data = result.FindStringByDottedPath(
          "result.result.value.screenshotResult")) {
    WriteFile(std::move(screenshot_file_path_), std::move(*base64_data));
  }

  if (std::string* base64_data = result.FindStringByDottedPath(
          "result.result.value.printToPdfResult")) {
    WriteFile(std::move(pdf_file_path_), std::move(*base64_data));
  }

  if (!write_file_tasks_in_flight_) {
    PostDone();
  }
}

void HeadlessCommandHandler::WriteFile(base::FilePath file_path,
                                       std::string base64_file_data) {
  std::string file_data;
  CHECK(base::Base64Decode(base64_file_data, &file_data));

  if (io_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&WriteFileTask, std::move(file_path),
                         std::move(file_data)),
          base::BindOnce(&HeadlessCommandHandler::OnWriteFileDone,
                         base::Unretained(this)))) {
    ++write_file_tasks_in_flight_;
  }
}

void HeadlessCommandHandler::OnWriteFileDone(bool success) {
  DCHECK_GT(write_file_tasks_in_flight_, 0) << write_file_tasks_in_flight_;

  if (!success) {
    result_ = Result::kWriteFileError;
  }

  if (!--write_file_tasks_in_flight_) {
    Done();
  }
}

void HeadlessCommandHandler::PostDone() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&HeadlessCommandHandler::Done, base::Unretained(this)));
}

void HeadlessCommandHandler::Done() {
  devtools_client_.DetachClient();
  browser_devtools_client_.DetachClient();

  Result result = result_;
  DoneCallback done_callback(std::move(done_callback_));
  delete this;
  std::move(done_callback).Run(result);

  if (GetGlobalDoneCallback()) {
    std::move(GetGlobalDoneCallback()).Run(result);
  }
}

// static
void HeadlessCommandHandler::SetDoneCallbackForTesting(
    DoneCallback done_callback) {
  GetGlobalDoneCallback() = std::move(done_callback);
}

}  // namespace headless
