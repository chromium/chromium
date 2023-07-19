// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_source.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "net/base/mime_util.h"

namespace {
constexpr char kDefaultMime[] = "text/html";
constexpr base::FilePath::CharType kMakoRoot[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/mako");
constexpr base::FilePath::CharType kOrcaHTML[] =
    FILE_PATH_LITERAL("html/mako.html");

std::string ReadFile(const base::FilePath& relative_path) {
  CHECK(!relative_path.ReferencesParent());

  base::FilePath path = base::FilePath(kMakoRoot).Append(relative_path);
  std::string content;
  bool result = base::ReadFileToString(path, &content);

  DCHECK(result) << path;
  return content;
}

}  // namespace

namespace ash {

MakoSource::MakoSource() noexcept = default;

std::string MakoSource::GetSource() {
  return ash::kChromeUIMakoHost;
}

void MakoSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  base::FilePath path(url.path().substr(1));
  if (path.empty()) {
    path = base::FilePath(kOrcaHTML);
  }

  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(ReadFile(path)));
}

std::string MakoSource::GetMimeType(const GURL& url) {
  std::string mime_type(kDefaultMime);
  std::string extension = base::FilePath(url.path_piece()).Extension();
  if (!extension.empty()) {
    net::GetWellKnownMimeTypeFromExtension(extension.substr(1), &mime_type);
  }
  return mime_type;
}

std::string MakoSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  switch (directive) {
    case network::mojom::CSPDirectiveName::TrustedTypes:
      // Intentional space at end - things are appended to this.
      return "trusted-types goog#html polymer_resin lit-html; ";
    default:
      return content::URLDataSource::GetContentSecurityPolicy(directive);
  }
}
}  // namespace ash
