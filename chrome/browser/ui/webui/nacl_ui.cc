// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nacl_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/webplugininfo.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;
using base::UserMetricsAction;
using content::BrowserThread;
using content::PluginService;
using content::WebUIMessageHandler;

namespace {

void CreateAndAddNaClUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUINaClHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer-html-literal "
      "polymer-template-event-attribute-policy;");

  source->UseStringsJs();
  source->AddResourcePath("about_nacl.css", IDR_ABOUT_NACL_CSS);
  source->AddResourcePath("about_nacl.js", IDR_ABOUT_NACL_JS);
  source->SetDefaultResource(IDR_ABOUT_NACL_HTML);
}

////////////////////////////////////////////////////////////////////////////////
//
// NaClDomHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for JavaScript messages for the about:flags page.
class NaClDomHandler : public WebUIMessageHandler {
 public:
  NaClDomHandler();

  NaClDomHandler(const NaClDomHandler&) = delete;
  NaClDomHandler& operator=(const NaClDomHandler&) = delete;

  ~NaClDomHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  // Callback for the "requestNaClInfo" message.
  void HandleRequestNaClInfo(const base::Value::List& args);

  // Callback for the NaCl plugin information.
  void OnGotPlugins(const std::vector<content::WebPluginInfo>& plugins);

  // A helper callback that receives the result of checking if PNaCl path
  // exists and checking the PNaCl |version|. |is_valid| is true if the PNaCl
  // path that was returned by PathService is valid, and false otherwise.
  void DidCheckPathAndVersion(const std::string* version, bool is_valid);

  // Called when enough information is gathered to return data back to the page.
  void MaybeRespondToPage();

  // Helper for MaybeRespondToPage -- called after enough information
  // is gathered.
  base::Value::Dict GetPageInformation();

  // Returns whether the specified plugin is enabled.
  bool isPluginEnabled(size_t plugin_index);

  // Adds information regarding the operating system and chrome version to list.
  void AddOperatingSystemInfo(base::Value::List* list);

  // Adds the list of plugins for NaCl to list.
  void AddPluginList(base::Value::List* list);

  // Adds the information relevant to PNaCl (e.g., enablement, paths, version)
  // to the list.
  void AddPnaclInfo(base::Value::List* list);

  // Adds the information relevant to NaCl to list.
  void AddNaClInfo(base::Value::List* list);

  // The callback ID for requested data.
  std::string callback_id_;

  // Whether the plugin information is ready.
  bool has_plugin_info_;

  // Whether PNaCl path was validated. PathService can return a path
  // that does not exists, so it needs to be validated.
  bool pnacl_path_validated_;
  bool pnacl_path_exists_;
  std::string pnacl_version_string_;

  base::WeakPtrFactory<NaClDomHandler> weak_ptr_factory_{this};
};

NaClDomHandler::NaClDomHandler()
    : has_plugin_info_(false),
      pnacl_path_validated_(false),
      pnacl_path_exists_(false) {
}

NaClDomHandler::~NaClDomHandler() = default;

void NaClDomHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestNaClInfo",
      base::BindRepeating(&NaClDomHandler::HandleRequestNaClInfo,
                          base::Unretained(this)));
}

void NaClDomHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AddPair(base::Value::List* list,
             const std::u16string& key,
             const std::u16string& value) {
  // clang-format off
  list->Append(
      base::Value::Dict()
          .Set("key", key)
          .Set("value", value));
  // clang-format on
}

// Generate an empty data-pair which acts as a line break.
void AddLineBreak(base::Value::List* list) {
  AddPair(list, u"", u"");
}

bool NaClDomHandler::isPluginEnabled(size_t plugin_index) {
  std::vector<content::WebPluginInfo> info_array;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), "application/x-nacl", false, &info_array, NULL);
  PluginPrefs* plugin_prefs =
      PluginPrefs::GetForProfile(Profile::FromWebUI(web_ui())).get();
  return (!info_array.empty() &&
          plugin_prefs->IsPluginEnabled(info_array[plugin_index]));
}

void NaClDomHandler::AddOperatingSystemInfo(base::Value::List* list) {
  // Obtain the Chrome version info.
  AddPair(
      list, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      ASCIIToUTF16(base::StrCat(
          {version_info::GetVersionNumber(), " (",
           chrome::GetChannelName(chrome::WithExtendedStable(true)), ")"})));

  // OS version information.
  std::string os_label(version_info::GetOSType());
  AddPair(list, l10n_util::GetStringUTF16(IDS_VERSION_UI_OS),
          ASCIIToUTF16(os_label));
  AddLineBreak(list);
}

void NaClDomHandler::AddPluginList(base::Value::List* list) {
  // Obtain the version of the NaCl plugin.
  std::vector<content::WebPluginInfo> info_array;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), "application/x-nacl", false, &info_array, NULL);
  std::u16string nacl_version;
  std::u16string nacl_key = u"NaCl plugin";
  if (info_array.empty()) {
    AddPair(list, nacl_key, u"Disabled");
  } else {
    // Only the 0th plugin is used.
    nacl_version =
        info_array[0].version + u" " + info_array[0].path.LossyDisplayName();
    if (!isPluginEnabled(0)) {
      nacl_version += u" (Disabled in profile prefs)";
    }

    AddPair(list, nacl_key, nacl_version);

    // Mark the rest as not used.
    for (size_t i = 1; i < info_array.size(); ++i) {
      nacl_version =
          info_array[i].version + u" " + info_array[i].path.LossyDisplayName();
      nacl_version += u" (not used)";
      if (!isPluginEnabled(i)) {
        nacl_version += u" (Disabled in profile prefs)";
      }
      AddPair(list, nacl_key, nacl_version);
    }
  }
  AddLineBreak(list);
}

void NaClDomHandler::AddPnaclInfo(base::Value::List* list) {
  // Display whether PNaCl is enabled.
  std::u16string pnacl_enabled_string = u"Enabled";
  if (!isPluginEnabled(0)) {
    pnacl_enabled_string = u"Disabled in profile prefs";
  }
  AddPair(list, u"Portable Native Client (PNaCl)", pnacl_enabled_string);

  // Obtain the version of the PNaCl translator.
  base::FilePath pnacl_path;
  bool got_path =
      base::PathService::Get(chrome::DIR_PNACL_COMPONENT, &pnacl_path);
  if (!got_path || pnacl_path.empty() || !pnacl_path_exists_) {
    AddPair(list, u"PNaCl translator", u"Not installed");
  } else {
    AddPair(list, u"PNaCl translator path", pnacl_path.LossyDisplayName());
    AddPair(list, u"PNaCl translator version",
            ASCIIToUTF16(pnacl_version_string_));
  }
  AddLineBreak(list);
}

void NaClDomHandler::AddNaClInfo(base::Value::List* list) {
  std::u16string nacl_enabled_string = u"Disabled";
  if (isPluginEnabled(0) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNaCl)) {
    nacl_enabled_string = u"Enabled by flag '--enable-nacl'";
  }
  AddPair(list, u"Native Client (non-portable, outside web store)",
          nacl_enabled_string);
  AddLineBreak(list);
}

void NaClDomHandler::HandleRequestNaClInfo(const base::Value::List& args) {
  CHECK(callback_id_.empty());
  CHECK_EQ(1U, args.size());
  callback_id_ = args[0].GetString();

  if (!has_plugin_info_) {
    PluginService::GetInstance()->GetPlugins(base::BindOnce(
        &NaClDomHandler::OnGotPlugins, weak_ptr_factory_.GetWeakPtr()));
  }

  // Force re-validation of PNaCl's path in the next call to
  // MaybeRespondToPage(), in case PNaCl went from not-installed
  // to installed since the request.
  pnacl_path_validated_ = false;
  AllowJavascript();
  MaybeRespondToPage();
}

void NaClDomHandler::OnGotPlugins(
    const std::vector<content::WebPluginInfo>& plugins) {
  has_plugin_info_ = true;
  MaybeRespondToPage();
}

base::Value::Dict NaClDomHandler::GetPageInformation() {
  // Store Key-Value pairs of about-information.
  base::Value::List list;
  // Display the operating system and chrome version information.
  AddOperatingSystemInfo(&list);
  // Display the list of plugins serving NaCl.
  AddPluginList(&list);
  // Display information relevant to PNaCl.
  AddPnaclInfo(&list);
  // Display information relevant to NaCl (non-portable.
  AddNaClInfo(&list);
  // naclInfo will take ownership of list, and clean it up on destruction.
  return base::Value::Dict().Set("naclInfo", std::move(list));
}

void NaClDomHandler::DidCheckPathAndVersion(const std::string* version,
                                            bool is_valid) {
  pnacl_path_validated_ = true;
  pnacl_path_exists_ = is_valid;
  pnacl_version_string_ = *version;
  MaybeRespondToPage();
}

void CheckVersion(const base::FilePath& pnacl_path, std::string* version) {
  base::FilePath pnacl_json_path =
      pnacl_path.AppendASCII("pnacl_public_pnacl_json");
  JSONFileValueDeserializer deserializer(pnacl_json_path);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root || !root->is_dict())
    return;

  // Now try to get the field. This may leave version empty if the
  // the "get" fails (no key, or wrong type).
  if (const std::string* ptr = root->GetDict().FindString("pnacl-version")) {
    if (base::IsStringASCII(*ptr))
      *version = *ptr;
  }
}

bool CheckPathAndVersion(std::string* version) {
  base::FilePath pnacl_path;
  bool got_path =
      base::PathService::Get(chrome::DIR_PNACL_COMPONENT, &pnacl_path);
  if (got_path && !pnacl_path.empty() && base::PathExists(pnacl_path)) {
    CheckVersion(pnacl_path, version);
    return true;
  }
  return false;
}

void NaClDomHandler::MaybeRespondToPage() {
  // Don't reply until everything is ready.  The page will show a 'loading'
  // message until then.
  if (callback_id_.empty() || !has_plugin_info_)
    return;

  if (!pnacl_path_validated_) {
    std::string* version_string = new std::string;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&CheckPathAndVersion, version_string),
        base::BindOnce(&NaClDomHandler::DidCheckPathAndVersion,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Owned(version_string)));
    return;
  }

  ResolveJavascriptCallback(base::Value(callback_id_), GetPageInformation());
  callback_id_.clear();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// NaClUI
//
///////////////////////////////////////////////////////////////////////////////

NaClUI::NaClUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  base::RecordAction(UserMetricsAction("ViewAboutNaCl"));

  web_ui->AddMessageHandler(std::make_unique<NaClDomHandler>());

  // Set up the about:nacl source.
  CreateAndAddNaClUIHTMLSource(Profile::FromWebUI(web_ui));
}
