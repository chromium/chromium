// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/filename_generation/filename_generation.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace filename_generation {

#define FPL FILE_PATH_LITERAL
#define HTML_EXTENSION ".html"
#if BUILDFLAG(IS_WIN)
#define FPL_HTML_EXTENSION L".html"
#else
#define FPL_HTML_EXTENSION ".html"
#endif

namespace {

base::FilePath GetLongNamePathInDirectory(
    int max_length,
    const base::FilePath::CharType* suffix,
    const base::FilePath& dir) {
  base::FilePath::StringType name(max_length, FILE_PATH_LITERAL('a'));
  base::FilePath path = dir.Append(name + suffix).NormalizePathSeparators();
  return path;
}

}  // namespace

static const struct {
  const base::FilePath::CharType* page_title;
  const base::FilePath::CharType* expected_name;
} kExtensionTestCases[] = {
    // Extension is preserved if it is already proper for HTML.
    {FPL("filename.html"), FPL("filename.html")},
    {FPL("filename.HTML"), FPL("filename.HTML")},
    {FPL("filename.XHTML"), FPL("filename.XHTML")},
    {FPL("filename.xhtml"), FPL("filename.xhtml")},
    {FPL("filename.htm"), FPL("filename.htm")},
    // ".htm" is added if the extension is improper for HTML.
    {FPL("hello.world"), FPL("hello.world") FPL_HTML_EXTENSION},
    {FPL("hello.txt"), FPL("hello.txt") FPL_HTML_EXTENSION},
    {FPL("is.html.good"), FPL("is.html.good") FPL_HTML_EXTENSION},
    // ".htm" is added if the name doesn't have an extension.
    {FPL("helloworld"), FPL("helloworld") FPL_HTML_EXTENSION},
    {FPL("helloworld."), FPL("helloworld.") FPL_HTML_EXTENSION},
};

// Crashing on Windows, see http://crbug.com/79365
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestEnsureHtmlExtension DISABLED_TestEnsureHtmlExtension
#else
#define MAYBE_TestEnsureHtmlExtension TestEnsureHtmlExtension
#endif
TEST(FilenameGenerationTest, MAYBE_TestEnsureHtmlExtension) {
  for (size_t i = 0; i < std::size(kExtensionTestCases); ++i) {
    base::FilePath original = base::FilePath(kExtensionTestCases[i].page_title);
    base::FilePath expected =
        base::FilePath(kExtensionTestCases[i].expected_name);
    base::FilePath actual = EnsureHtmlExtension(original);
    EXPECT_EQ(expected.value(), actual.value())
        << "Failed for page title: " << kExtensionTestCases[i].page_title;
  }
}

// Crashing on Windows, see http://crbug.com/79365
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestEnsureMimeExtension DISABLED_TestEnsureMimeExtension
#else
#define MAYBE_TestEnsureMimeExtension TestEnsureMimeExtension
#endif
TEST(FilenameGenerationTest, MAYBE_TestEnsureMimeExtension) {
  static const struct {
    const base::FilePath::CharType* page_title;
    const base::FilePath::CharType* expected_name;
    const char* contents_mime_type;
  } kExtensionTests[] = {
    {FPL("filename.html"), FPL("filename.html"), "text/html"},
    {FPL("filename.htm"), FPL("filename.htm"), "text/html"},
    {FPL("filename.xhtml"), FPL("filename.xhtml"), "text/html"},
#if BUILDFLAG(IS_WIN)
    {FPL("filename"), FPL("filename.htm"), "text/html"},
#else   // BUILDFLAG(IS_WIN)
    {FPL("filename"), FPL("filename.html"), "text/html"},
#endif  // BUILDFLAG(IS_WIN)
    {FPL("filename.html"), FPL("filename.html"), "text/xml"},
    {FPL("filename.xml"), FPL("filename.xml"), "text/xml"},
    {FPL("filename"), FPL("filename.xml"), "text/xml"},
    {FPL("filename.xhtml"), FPL("filename.xhtml"), "application/xhtml+xml"},
    {FPL("filename.html"), FPL("filename.html"), "application/xhtml+xml"},
    {FPL("filename"), FPL("filename.xhtml"), "application/xhtml+xml"},
    {FPL("filename.txt"), FPL("filename.txt"), "text/plain"},
    {FPL("filename"), FPL("filename.txt"), "text/plain"},
    {FPL("filename.css"), FPL("filename.css"), "text/css"},
    {FPL("filename"), FPL("filename.css"), "text/css"},
    {FPL("filename.mhtml"), FPL("filename.mhtml"), "multipart/related"},
    {FPL("filename.html"), FPL("filename.html.mhtml"), "multipart/related"},
    {FPL("filename.txt"), FPL("filename.txt.mhtml"), "multipart/related"},
    {FPL("filename"), FPL("filename.mhtml"), "multipart/related"},
    {FPL("filename.abc"), FPL("filename.abc"), "unknown/unknown"},
    {FPL("filename"), FPL("filename"), "unknown/unknown"},
  };
  for (uint32_t i = 0; i < std::size(kExtensionTests); ++i) {
    base::FilePath original = base::FilePath(kExtensionTests[i].page_title);
    base::FilePath expected = base::FilePath(kExtensionTests[i].expected_name);
    std::string mime_type(kExtensionTests[i].contents_mime_type);
    base::FilePath actual = EnsureMimeExtension(original, mime_type);
    EXPECT_EQ(expected.value(), actual.value())
        << "Failed for page title: " << kExtensionTests[i].page_title
        << " MIME:" << mime_type;
  }
}

// Test that the suggested names generated are reasonable:
// If the name is a URL, retrieve only the path component since the path name
// generation code will turn the entire URL into the file name leading to bad
// extension names. For example, a page with no title and a URL:
// http://www.foo.com/a/path/name.txt will turn into file:
// "http www.foo.com a path name.txt", when we want to save it as "name.txt".

static const struct GenerateFilenameTestCase {
  const char* page_url;
  const std::u16string page_title;
  const base::FilePath::CharType* expected_name;
  bool ensure_html_extension;
} kGenerateFilenameCases[] = {
    // Title overrides the URL.
    {"http://foo.com", u"A page title", FPL("A page title") FPL_HTML_EXTENSION,
     true},
    // Extension is preserved.
    {"http://foo.com", u"A page title with.ext", FPL("A page title with.ext"),
     false},
    // If the title matches the URL, use the last component of the URL.
    {"http://foo.com/bar", u"foo.com/bar", FPL("bar"), false},
    // A URL with escaped special characters, when title matches the URL.
    {"http://foo.com/%40.txt", u"foo.com/%40.txt", FPL("@.txt"), false},
    // A URL with unescaped special characters, when title matches the URL.
    {"http://foo.com/@.txt", u"foo.com/@.txt", FPL("@.txt"), false},
    // A URL with punycode in the host name, when title matches the URL.
    {"http://xn--bcher-kva.com", u"bücher.com", FPL("bücher.com"), false},
    // If the title matches the URL, but there is no "filename" component,
    // use the domain.
    {"http://foo.com", u"foo.com", FPL("foo.com"), false},
    // Make sure fuzzy matching works.
    {"http://foo.com/bar", u"foo.com/bar", FPL("bar"), false},
    // A URL-like title that does not match the title is respected in full.
    {"http://foo.com", u"http://www.foo.com/path/title.txt",
     FPL("http___www.foo.com_path_title.txt"), false},
};

// Crashing on Windows, see http://crbug.com/79365
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestGenerateFilename DISABLED_TestGenerateFilename
#else
#define MAYBE_TestGenerateFilename TestGenerateFilename
#endif
TEST(FilenameGenerationTest, MAYBE_TestGenerateFilename) {
  for (size_t i = 0; i < std::size(kGenerateFilenameCases); ++i) {
    base::FilePath save_name = GenerateFilename(
        kGenerateFilenameCases[i].page_title,
        GURL(kGenerateFilenameCases[i].page_url),
        kGenerateFilenameCases[i].ensure_html_extension, std::string());
    EXPECT_EQ(kGenerateFilenameCases[i].expected_name, save_name.value())
        << "Test case " << i;
  }
}

TEST(FilenameGenerationTest, TestBasicTruncation) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  int max_length = base::GetMaximumPathComponentLength(temp_dir.GetPath());
  ASSERT_NE(-1, max_length);

  base::FilePath::StringType extension(FILE_PATH_LITERAL(".txt"));
  base::FilePath path(GetLongNamePathInDirectory(
      max_length, FILE_PATH_LITERAL(".txt"), temp_dir.GetPath()));
  base::FilePath truncated_path = path;

// The file path will only be truncated o the platforms that have known
// encoding. Otherwise no truncation will be performed.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS_ASH)
  // The file name length is truncated to max_length.
  EXPECT_TRUE(TruncateFilename(&truncated_path, max_length));
  EXPECT_EQ(size_t(max_length), truncated_path.BaseName().value().size());
#else
  EXPECT_FALSE(TruncateFilename(&truncated_path, max_length));
  EXPECT_EQ(truncated_path, path);
  EXPECT_LT(size_t(max_length), truncated_path.BaseName().value().size());
#endif
  // But the extension is kept unchanged.
  EXPECT_EQ(path.Extension(), truncated_path.Extension());
}

TEST(FilenameGenerationTest, TestTruncationFail) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  int max_length = base::GetMaximumPathComponentLength(temp_dir.GetPath());
  ASSERT_NE(-1, max_length);

  base::FilePath path(
      (FILE_PATH_LITERAL("a.") + base::FilePath::StringType(max_length, 'b'))
          .c_str());
  path = temp_dir.GetPath().Append(path);

  base::FilePath truncated_path = path;

  // We cannot truncate a path with very long extension. This will fail and no
  // truncation will be performed on all platforms.
  EXPECT_FALSE(TruncateFilename(&truncated_path, max_length));
  EXPECT_EQ(truncated_path, path);
}

}  // filename_generation
