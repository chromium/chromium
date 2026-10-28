// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/process_extractor.h"

#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/test/scoped_path_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

class ProcessExtractorTest : public testing::Test {
 protected:
  base::ScopedPathOverride home_override_{
      base::DIR_HOME, base::FilePath(FILE_PATH_LITERAL("/Users/alice")),
      /*is_absolute=*/true, /*create=*/false};
};

TEST_F(ProcessExtractorTest, ProcessTreeAndNodeConstruction) {
  ProcessTree tree;
  EXPECT_TRUE(tree.empty());

  tree.process_nodes.emplace_back(1234, 567, "/usr/local/bin/node",
                                  "/Users/<USER>/agent.js");
  EXPECT_FALSE(tree.empty());
  EXPECT_EQ(tree.process_nodes[0].process_id,
            static_cast<base::ProcessId>(1234));
  EXPECT_EQ(tree.process_nodes[0].parent_process_id,
            static_cast<base::ProcessId>(567));
  EXPECT_EQ(tree.process_nodes[0].executable_path, "/usr/local/bin/node");
  EXPECT_EQ(tree.process_nodes[0].script_path, "/Users/<USER>/agent.js");
}

TEST_F(ProcessExtractorTest, ScrubsHomeDirectoriesAndTruncatesUtf8) {
  // Standard OS home directory prefix and exact home directory match.
  EXPECT_EQ(SanitizeProcessPath("/Users/alice/projects/agent.js"),
            "/Users/<USER>/projects/agent.js");
  EXPECT_EQ(SanitizeProcessPath("/Users/alice"), "/Users/<USER>");

  // Repeated internal slashes in home prefixes are normalized by
  // base::FilePath.
  EXPECT_EQ(SanitizeProcessPath("/Users//alice//agent.js"),
            "/Users/<USER>/agent.js");

  // Shared/guest directories outside DIR_HOME are preserved.
  EXPECT_EQ(SanitizeProcessPath("/Users/Shared/agent/run.js"),
            "/Users/Shared/agent/run.js");
  EXPECT_EQ(SanitizeProcessPath("/Users/Guest/agent/run.js"),
            "/Users/Guest/agent/run.js");

  // Mid-path `/Users/` directories are untouched.
  EXPECT_EQ(
      SanitizeProcessPath("/Users/alice/webapp/src/routes/Users/profile.js"),
      "/Users/<USER>/webapp/src/routes/Users/profile.js");
  EXPECT_EQ(SanitizeProcessPath("/opt/webapp/src/routes/Users/profile.js"),
            "/opt/webapp/src/routes/Users/profile.js");

  // `SanitizeProcessNodePaths()` scrubs both executable and script paths on a
  // `ProcessNode`.
  ProcessNode node(1234, 567, "/Users/alice/.nvm/versions/node/v20/bin/node",
                   "/Users/alice/workspace/agent.js");
  SanitizeProcessNodePaths(node);
  EXPECT_EQ(node.executable_path,
            "/Users/<USER>/.nvm/versions/node/v20/bin/node");
  EXPECT_EQ(node.script_path, "/Users/<USER>/workspace/agent.js");

  // System paths outside home directories remain untouched.
  EXPECT_EQ(SanitizeProcessPath("/usr/local/bin/node"), "/usr/local/bin/node");
  EXPECT_EQ(SanitizeProcessPath("/opt/homebrew/bin/python3"),
            "/opt/homebrew/bin/python3");

  // Paths exceeding `kMaxProcessPathLength` (1024 bytes) drop the beginning of
  // the string while preserving the trailing filename and respecting UTF-8
  // codepoint boundaries.
  std::string tail = "/" + std::string(1017, 'a') + "/node";
  ASSERT_EQ(tail.size(), 1023u);
  // Prepend a prefix ending in a 2-byte UTF-8 character ("\xC3\xA9") such that
  // `path.size() - 1024` lands on the second (continuation) byte '\xA9'.
  std::string long_utf8_path = "/Users/alice/\xC3\xA9" + tail;
  EXPECT_EQ(SanitizeProcessPath(long_utf8_path), tail);

  // Fails closed (returns empty string) if DIR_HOME has no parent directory
  // component or cannot be resolved.
  base::ScopedPathOverride root_home(base::DIR_HOME,
                                     base::FilePath(FILE_PATH_LITERAL("/")),
                                     /*is_absolute=*/true, /*create=*/false);
  EXPECT_TRUE(SanitizeProcessPath("/Users/alice/projects/agent.js").empty());
}

TEST_F(ProcessExtractorTest,
       ExtractsScriptAndScrubsHomeForAllowlistedInterpretersOnly) {
  ProcessNode node(
      1234, 567, "/Users/alice/.nvm/versions/node/v20/bin/node",
      ExtractInterpreterScript("/Users/alice/.nvm/versions/node/v20/bin/node",
                               {"node", "/Users/alice/agent.js"}));
  SanitizeProcessNodePaths(node);
  EXPECT_EQ(node.executable_path,
            "/Users/<USER>/.nvm/versions/node/v20/bin/node");
  EXPECT_EQ(node.script_path, "/Users/<USER>/agent.js");

  // Flags, bare flag values (`dotenv/config`, `warn`, `4096`), and non-script
  // files (`/Users/alice/.env`, `tsconfig.json`) before the script target are
  // skipped structurally without a hardcoded flag list.
  EXPECT_EQ(ExtractInterpreterScript("/usr/local/bin/node",
                                     {"node", "--no-warnings", "app.js"}),
            "app.js");
  EXPECT_EQ(ExtractInterpreterScript(
                "/usr/local/bin/node",
                {"node", "--inspect=9229", "-r", "dotenv/config", "--env-file",
                 "/Users/alice/.env", "--unhandled-rejections", "warn",
                 "/Users/alice/runner.mjs", "--headless"}),
            "/Users/<USER>/runner.mjs");

  // Deno and Bun subcommand handling (`run`, `test`, `x`, `--config` vs
  // `eval`/`repl`).
  EXPECT_EQ(
      ExtractInterpreterScript("/opt/homebrew/bin/deno",
                               {"deno", "run", "--config", "tsconfig.json",
                                "--allow-all", "/Users/alice/agent.ts"}),
      "/Users/<USER>/agent.ts");
  EXPECT_EQ(ExtractInterpreterScript(
                "/opt/homebrew/bin/deno",
                {"deno", "eval", "console.log(Deno.env.get('SECRET'))"}),
            "");
  EXPECT_EQ(
      ExtractInterpreterScript("/opt/homebrew/bin/deno", {"deno", "repl"}), "");
  EXPECT_EQ(ExtractInterpreterScript("/Users/alice/.bun/bin/bun",
                                     {"bun", "run", "/Users/alice/agent.ts"}),
            "/Users/<USER>/agent.ts");

  // Inline code flags (`-c`, `-lc`, `-ec`, `-s`, `-e`, `-E`, `--eval`, `-p`,
  // `-m`, `-i`, bare `-` stdin) omit `script_path`.
  EXPECT_EQ(ExtractInterpreterScript("/usr/local/bin/node",
                                     {"node", "-", "ignored.js"}),
            "");
  EXPECT_EQ(ExtractInterpreterScript("/opt/homebrew/bin/python3",
                                     {"python3", "-c", "import os; print(1)"}),
            "");
  EXPECT_EQ(ExtractInterpreterScript("/opt/homebrew/bin/python3",
                                     {"python3", "-cimport os; print(1)"}),
            "");
  EXPECT_EQ(ExtractInterpreterScript(
                "/usr/local/bin/node",
                {"node", "-e", "console.log(process.env.SECRET)"}),
            "");
  EXPECT_EQ(
      ExtractInterpreterScript("/usr/local/bin/node",
                               {"node", "-econsole.log(process.env.SECRET)"}),
      "");
  EXPECT_EQ(ExtractInterpreterScript("/usr/local/bin/node",
                                     {"node", "--input-type", "module", "-e",
                                      "console.log(process.env.SECRET)"}),
            "");
  EXPECT_EQ(ExtractInterpreterScript(
                "/usr/local/bin/node",
                {"node", "--eval", "console.log(1)", "ignored.js"}),
            "");
  EXPECT_EQ(ExtractInterpreterScript("/bin/bash",
                                     {"bash", "-c", "curl http://example.com"}),
            "");
  EXPECT_EQ(ExtractInterpreterScript("/bin/sh", {"sh", "-s", "arg1", "arg2"}),
            "");
  EXPECT_EQ(ExtractInterpreterScript(
                "/bin/zsh", {"zsh", "-lc",
                             "node /Users/alice/agent.js --token=SECRET_KEY"}),
            "");
  EXPECT_EQ(
      ExtractInterpreterScript(
          "/bin/bash", {"bash", "-ec", "python3 bot.py --api-key=SECRET"}),
      "");
  EXPECT_EQ(ExtractInterpreterScript("/bin/bash",
                                     {"bash", "-e", "/Users/alice/run.sh"}),
            "/Users/<USER>/run.sh");
  EXPECT_EQ(ExtractInterpreterScript(
                "/bin/bash", {"bash", "-o", "pipefail", "/Users/alice/run.sh"}),
            "/Users/<USER>/run.sh");

  // Arguments passed after the script target that resemble interpreter inline
  // flags (`-p`, `-c`, `-s`) are script arguments, not interpreter flags.
  EXPECT_EQ(ExtractInterpreterScript(
                "/usr/local/bin/node",
                {"node", "/Users/alice/server.js", "-p", "8080"}),
            "/Users/<USER>/server.js");
  EXPECT_EQ(ExtractInterpreterScript(
                "/opt/homebrew/bin/python3",
                {"python3", "/Users/alice/agent.py", "-c", "config.yaml"}),
            "/Users/<USER>/agent.py");
  EXPECT_EQ(ExtractInterpreterScript("/bin/bash",
                                     {"bash", "/Users/alice/run.sh", "-s"}),
            "/Users/<USER>/run.sh");

  // Fails closed on URLs, tilde prefixes, parent-directory references,
  // extensionless paths, non-allowlisted extensions, and non-script `--` args.
  EXPECT_TRUE(ExtractInterpreterScript(
                  "/opt/homebrew/bin/deno",
                  {"deno", "run", "https://example.com/agent.ts?token=secret"})
                  .empty());
  EXPECT_TRUE(ExtractInterpreterScript("/usr/local/bin/node",
                                       {"node", "file:///Users/alice/agent.js"})
                  .empty());
  EXPECT_TRUE(
      ExtractInterpreterScript("/usr/local/bin/node", {"node", "~/agent.js"})
          .empty());
  EXPECT_TRUE(ExtractInterpreterScript("/usr/local/bin/node",
                                       {"node", "../alice/agent.js"})
                  .empty());
  EXPECT_TRUE(ExtractInterpreterScript("/usr/local/bin/node",
                                       {"node", "/Users/alice/custom_binary"})
                  .empty());
  EXPECT_TRUE(ExtractInterpreterScript("/usr/local/bin/node",
                                       {"node", "/Users/alice/secrets.txt"})
                  .empty());
  EXPECT_TRUE(
      ExtractInterpreterScript("/usr/local/bin/node",
                               {"node", "--", "/Users/alice/not_a_script"})
          .empty());
  EXPECT_EQ(ExtractInterpreterScript("/usr/local/bin/node",
                                     {"node", "--", "/Users/alice/agent.js"}),
            "/Users/<USER>/agent.js");

  // Long username that exceeds `kMaxProcessPathLength` before scrubbing but
  // fits after replacing with `<USER>` should not truncate the path.
  {
    std::string long_user(50, 'u');
    base::ScopedPathOverride long_user_home(
        base::DIR_HOME, base::FilePath::FromUTF8Unsafe("/Users/" + long_user),
        /*is_absolute=*/true, /*create=*/false);
    std::string tail_dir(kMaxProcessPathLength - 25, 'd');
    std::string raw_long_script =
        "/Users/" + long_user + "/" + tail_dir + "/agent.js";
    ASSERT_GT(raw_long_script.size(), kMaxProcessPathLength);
    std::string extracted_long = ExtractInterpreterScript(
        "/usr/local/bin/node", {"node", raw_long_script});
    EXPECT_EQ(extracted_long, "/Users/<USER>/" + tail_dir + "/agent.js");
  }

  // Fails closed if `base::DIR_HOME` cannot be resolved.
  {
    base::ScopedPathOverride root_home(base::DIR_HOME,
                                       base::FilePath(FILE_PATH_LITERAL("/")),
                                       /*is_absolute=*/true, /*create=*/false);
    EXPECT_TRUE(ExtractInterpreterScript("/usr/local/bin/node",
                                         {"node", "/Users/alice/agent.js"})
                    .empty());
  }

  // Non-interpreter binaries omit `script_path`.
  EXPECT_FALSE(IsAllowlistedInterpreter(
      "/Applications/CustomAgent.app/Contents/MacOS/CustomAgent"));
  EXPECT_EQ(ExtractInterpreterScript(
                "/Applications/CustomAgent.app/Contents/MacOS/CustomAgent",
                {"CustomAgent", "/Users/alice/agent.js", "--flag"}),
            "");

  // All allowlisted interpreters, version-suffixed variants, and `.exe`
  // binaries are recognized.
  for (std::string_view bin :
       {"node", "nodejs", "node20", "node.exe", "python", "python2",
        "python2.7", "python3", "python3.12", "python3.exe", "bash", "sh",
        "zsh", "ruby", "ruby3.2", "perl", "perl5.34", "deno", "bun"}) {
    EXPECT_TRUE(IsAllowlistedInterpreter(bin)) << bin;
  }
}

}  // namespace

}  // namespace enterprise_connectors
