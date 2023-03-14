// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/command_handler/headless_command_handler.h"

#include <cstdint>
#include <iostream>
#include <map>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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

content::WebUIDataSource* CreateHeadlessHostDataSource() {
  EnsureHeadlessCommandResources();

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeHeadlessHost);
  DCHECK(source);

  source->AddResourcePath(kHeadlessCommandHtml, IDR_HEADLESS_COMMAND_HTML);
  source->AddResourcePath(kHeadlessCommandJs, IDR_HEADLESS_COMMAND_JS);

  return source;
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
    if (command_line->HasSwitch(switches::kNoPDFHeaderFooter) ||
        command_line->HasSwitch(switches::kPrintToPDFNoHeaderDeprecated)) {
      params.Set("noHeaderFooter", true);
    }

    if (command_line->HasSwitch(switches::kPrintToPDFNoHeaderDeprecated)) {
      LOG(WARNING) << "--" << switches::kPrintToPDFNoHeaderDeprecated
                   << " is deprecated, use --" << switches::kNoPDFHeaderFooter;
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

    static const std::map<const base::FilePath::StringType, const char*>
        kImageFileTypes{
            {FILE_PATH_LITERAL(".jpeg"), "jpeg"},
            {FILE_PATH_LITERAL(".jpg"), "jpeg"},
            {FILE_PATH_LITERAL(".png"), "png"},
            {FILE_PATH_LITERAL(".webp"), "webp"},
        };

    auto it = kImageFileTypes.find(extension);
    if (it == kImageFileTypes.cend()) {
      LOG(ERROR) << "Unsupported screenshot image file type: "
                 << path.FinalExtension();
      return false;
    }

    base::Value::Dict params;
    params.Set("format", it->second);
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

void WriteFileTask(base::FilePath file_path, std::string file_data) {
  auto file_span = base::make_span(
      reinterpret_cast<const uint8_t*>(file_data.data()), file_data.size());
  if (base::WriteFile(file_path, file_span)) {
    std::cerr << file_data.size() << " bytes written to file " << file_path
              << std::endl;
  } else {
    PLOG(ERROR) << "Failed to write file " << file_path;
  }
}

void WriteFile(base::FilePath file_path,
               std::string base64_file_data,
               scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  std::string file_data;
  CHECK(base::Base64Decode(base64_file_data, &file_data));

  io_task_runner->PostTask(FROM_HERE,
                           base::BindOnce(&WriteFileTask, std::move(file_path),
                                          std::move(file_data)));
}

}  // namespace

HeadlessCommandHandler::HeadlessCommandHandler(
    content::WebContents* web_contents,
    GURL target_url,
    DoneCallback done_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : web_contents_(web_contents),
      target_url_(std::move(target_url)),
      done_callback_(std::move(done_callback)),
      io_task_runner_(std::move(io_task_runner)) {
  DCHECK(web_contents_);
  DCHECK(io_task_runner_);

  // Load command execution harness resources and create URL data source
  // for chrome://headless.
  content::WebUIDataSource::Add(web_contents_->GetBrowserContext(),
                                CreateHeadlessHostDataSource());

  content::WebContentsObserver::Observe(web_contents_);

  browser_devtools_client_.AttachToBrowser();
  devtools_client_.AttachToWebContents(web_contents_);
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
      switches::kPrintToPDFNoHeaderDeprecated,
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
  HeadlessCommandHandler* command_handler =
      new HeadlessCommandHandler(web_contents, std::move(target_url),
                                 std::move(done_callback), io_task_runner);

  command_handler->ExecuteCommands();
}

void HeadlessCommandHandler::ExecuteCommands() {
  // Expose DevTools protocol to the target.
  base::Value::Dict params;
  params.Set("targetId", devtools_client_.GetTargetId());
  browser_devtools_client_.SendCommand("Target.exposeDevToolsProtocol",
                                       std::move(params));

  // Set up Inspector domain.
  devtools_client_.AddEventHandler(
      "Inspector.targetCrashed",
      base::BindRepeating(&HeadlessCommandHandler::OnTargetCrashed,
                          base::Unretained(this)));
  devtools_client_.SendCommand("Inspector.enable");
}

void HeadlessCommandHandler::DocumentOnLoadCompletedInPrimaryMainFrame() {
  base::Value::Dict commands;
  if (!GetCommandDictAndOutputPaths(&commands, &pdf_file_path_,
                                    &screenshot_file_path_) ||
      commands.empty()) {
    Done();
    return;
  }

  commands.Set("targetUrl", target_url_.spec());

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
  CHECK(false);
}

void HeadlessCommandHandler::OnTargetCrashed(const base::Value::Dict&) {
  LOG(ERROR) << "Abnormal renderer termination.";
  Done();
}

void HeadlessCommandHandler::OnCommandsResult(base::Value::Dict result) {
  if (std::string* dom_dump =
          result.FindStringByDottedPath("result.result.value.dumpDomResult")) {
    std::cout << *dom_dump << std::endl;
  }

  if (std::string* base64_data = result.FindStringByDottedPath(
          "result.result.value.screenshotResult")) {
    WriteFile(std::move(screenshot_file_path_), std::move(*base64_data),
              io_task_runner_);
  }

  if (std::string* base64_data = result.FindStringByDottedPath(
          "result.result.value.printToPdfResult")) {
    WriteFile(std::move(pdf_file_path_), std::move(*base64_data),
              io_task_runner_);
  }

  Done();
}

void HeadlessCommandHandler::Done() {
  DCHECK(web_contents_);
  devtools_client_.DetachClient();
  browser_devtools_client_.DetachClient();

  DoneCallback done_callback(std::move(done_callback_));
  delete this;
  std::move(done_callback).Run();
}

}  // namespace headless
