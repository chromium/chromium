// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/plugin_uma.h"

#include <algorithm>
#include <cstring>

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_constants.h"

namespace {

// String we will use to convert mime type to plugin type.
const char kWindowsMediaPlayerType[] = "application/x-mplayer2";
const char kSilverlightTypePrefix[] = "application/x-silverlight";
const char kRealPlayerTypePrefix[] = "audio/x-pn-realaudio";
const char kJavaTypeSubstring[] = "application/x-java-applet";
const char kQuickTimeType[] = "video/quicktime";

// Arrays containing file extensions connected with specific plugins.
// Note: THE ARRAYS MUST BE SORTED BECAUSE BINARY SEARCH IS USED ON THEM!
const char* const kWindowsMediaPlayerExtensions[] = {".asx"};

const char* const kRealPlayerExtensions[] = {".ra",  ".ram", ".rm",
                                             ".rmm", ".rmp", ".rpm"};

const char* const kQuickTimeExtensions[] = {".moov", ".mov", ".qif",
                                            ".qt",   ".qti", ".qtif"};

}  // namespace.

class UMASenderImpl : public PluginUMAReporter::UMASender {
  void SendPluginUMA(PluginUMAReporter::ReportType report_type,
                     PluginUMAReporter::PluginType plugin_type) override;
};

void UMASenderImpl::SendPluginUMA(PluginUMAReporter::ReportType report_type,
                                  PluginUMAReporter::PluginType plugin_type) {
  // UMA_HISTOGRAM_ENUMERATION requires constant histogram name. Use string
  // constants explicitly instead of trying to use variables for names.
  switch (report_type) {
    case PluginUMAReporter::MISSING_PLUGIN:
      UMA_HISTOGRAM_ENUMERATION("Plugin.MissingPlugins",
                                plugin_type,
                                PluginUMAReporter::PLUGIN_TYPE_MAX);
      break;
    case PluginUMAReporter::DISABLED_PLUGIN:
      UMA_HISTOGRAM_ENUMERATION("Plugin.DisabledPlugins",
                                plugin_type,
                                PluginUMAReporter::PLUGIN_TYPE_MAX);
      break;
    default:
      NOTREACHED();
  }
}

// static.
PluginUMAReporter* PluginUMAReporter::GetInstance() {
  return base::Singleton<PluginUMAReporter>::get();
}

void PluginUMAReporter::ReportPluginMissing(const std::string& plugin_mime_type,
                                            const GURL& plugin_src) {
  report_sender_->SendPluginUMA(MISSING_PLUGIN,
                                GetPluginType(plugin_mime_type, plugin_src));
}

void PluginUMAReporter::ReportPluginDisabled(
    const std::string& plugin_mime_type,
    const GURL& plugin_src) {
  report_sender_->SendPluginUMA(DISABLED_PLUGIN,
                                GetPluginType(plugin_mime_type, plugin_src));
}

PluginUMAReporter::PluginUMAReporter() : report_sender_(new UMASenderImpl()) {}

PluginUMAReporter::~PluginUMAReporter() {}

// static.
bool PluginUMAReporter::CompareCStrings(const char* first, const char* second) {
  return strcmp(first, second) < 0;
}

bool PluginUMAReporter::CStringArrayContainsCString(const char* const* array,
                                                    size_t array_size,
                                                    const char* str) {
  return std::binary_search(array, array + array_size, str, CompareCStrings);
}

void PluginUMAReporter::ExtractFileExtension(const GURL& src,
                                             std::string* extension) {
  std::string extension_file_path(src.ExtractFileName());
  if (extension_file_path.empty())
    extension_file_path = src.host();

  size_t last_dot = extension_file_path.find_last_of('.');
  if (last_dot != std::string::npos) {
    *extension = extension_file_path.substr(last_dot);
  } else {
    extension->clear();
  }

  *extension = base::ToLowerASCII(*extension);
}

PluginUMAReporter::PluginType PluginUMAReporter::GetPluginType(
    const std::string& plugin_mime_type,
    const GURL& plugin_src) {
  // If we know plugin's mime type, we use it to determine plugin's type. Else,
  // we try to determine plugin type using plugin source's extension.
  if (!plugin_mime_type.empty())
    return MimeTypeToPluginType(base::ToLowerASCII(plugin_mime_type));

  return SrcToPluginType(plugin_src);
}

PluginUMAReporter::PluginType PluginUMAReporter::SrcToPluginType(
    const GURL& src) {
  std::string file_extension;
  ExtractFileExtension(src, &file_extension);
  if (CStringArrayContainsCString(kWindowsMediaPlayerExtensions,
                                  std::size(kWindowsMediaPlayerExtensions),
                                  file_extension.c_str())) {
    return WINDOWS_MEDIA_PLAYER;
  }

  if (CStringArrayContainsCString(kQuickTimeExtensions,
                                  std::size(kQuickTimeExtensions),
                                  file_extension.c_str())) {
    return QUICKTIME;
  }

  if (CStringArrayContainsCString(kRealPlayerExtensions,
                                  std::size(kRealPlayerExtensions),
                                  file_extension.c_str())) {
    return REALPLAYER;
  }

  return UNSUPPORTED_EXTENSION;
}

PluginUMAReporter::PluginType PluginUMAReporter::MimeTypeToPluginType(
    const std::string& mime_type) {
  if (mime_type == kWindowsMediaPlayerType)
    return WINDOWS_MEDIA_PLAYER;

  size_t prefix_length = strlen(kSilverlightTypePrefix);
  if (strncmp(mime_type.c_str(), kSilverlightTypePrefix, prefix_length) == 0)
    return SILVERLIGHT;

  prefix_length = strlen(kRealPlayerTypePrefix);
  if (strncmp(mime_type.c_str(), kRealPlayerTypePrefix, prefix_length) == 0)
    return REALPLAYER;

  if (strstr(mime_type.c_str(), kJavaTypeSubstring))
    return JAVA;

  if (mime_type == kQuickTimeType)
    return QUICKTIME;

  if (mime_type == content::kBrowserPluginMimeType)
    return BROWSER_PLUGIN;

  return UNSUPPORTED_MIMETYPE;
}
