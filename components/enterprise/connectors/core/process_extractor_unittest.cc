// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/process_extractor.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/process/process_handle.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

std::vector<char> BuildKernProcArgs2Buffer(
    int argc,
    std::string_view exec_path,
    bool terminate_exec_path,
    size_t nul_padding_count,
    const std::vector<std::pair<std::string, bool>>& args) {
  std::vector<char> buf(sizeof(int), '\0');
  base::as_writable_bytes(base::span(buf).first<sizeof(int)>())
      .copy_from(base::byte_span_from_ref(argc));

  buf.insert(buf.end(), exec_path.begin(), exec_path.end());
  if (terminate_exec_path) {
    buf.push_back('\0');
  }
  buf.insert(buf.end(), nul_padding_count, '\0');

  for (const auto& [arg, terminate] : args) {
    buf.insert(buf.end(), arg.begin(), arg.end());
    if (terminate) {
      buf.push_back('\0');
    }
  }
  return buf;
}

std::optional<internal::RawProcessInfo> LookupProcessInMap(
    const std::map<base::ProcessId, internal::RawProcessInfo>& table,
    base::ProcessId pid) {
  auto it = table.find(pid);
  return (it == table.end()) ? std::nullopt : std::make_optional(it->second);
}

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

TEST_F(ProcessExtractorTest,
       ParsesValidAndRejectsMalformedKernProcArgs2Buffers) {
  std::vector<char> valid_buf = BuildKernProcArgs2Buffer(
      /*argc=*/3, "/usr/local/bin/node", /*terminate_exec_path=*/true,
      /*nul_padding_count=*/4,
      {{"node", true},
       {"/Users/alice/agent.js", true},
       {"--token=secret", true}});
  std::optional<internal::ParsedKernProcArgs2> parsed =
      internal::ParseKernProcArgs2Buffer(valid_buf);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->executable_path, "/usr/local/bin/node");
  ASSERT_EQ(parsed->argv.size(), 3u);
  EXPECT_EQ(parsed->argv[1], "/Users/alice/agent.js");

  // Reject tiny, oversized (>32 KB), non-positive `argc`, or unterminated
  // `executable_path` buffers.
  const char tiny[] = {'a', 'b'};
  EXPECT_FALSE(
      internal::ParseKernProcArgs2Buffer(base::span(tiny)).has_value());
  EXPECT_FALSE(
      internal::ParseKernProcArgs2Buffer(
          std::vector<char>(internal::kMaxKernProcArgs2BufferSize + 1, '\0'))
          .has_value());
  EXPECT_FALSE(internal::ParseKernProcArgs2Buffer(
                   BuildKernProcArgs2Buffer(0, "/usr/bin/python3", true, 1, {}))
                   .has_value());
  EXPECT_FALSE(internal::ParseKernProcArgs2Buffer(
                   BuildKernProcArgs2Buffer(1, "/usr/bin/node", false, 0, {}))
                   .has_value());

  // Unterminated trailing `argv` token is dropped while keeping valid tokens;
  // full tokens are preserved here so `ExtractInterpreterScript()` and
  // `SanitizeProcessNodePaths()` inspect the leading prefix before
  // front-truncation.
  std::optional<internal::ParsedKernProcArgs2> partial =
      internal::ParseKernProcArgs2Buffer(
          BuildKernProcArgs2Buffer(3, "/usr/local/bin/node", true, 1,
                                   {{"node", true},
                                    {std::string(2000, 'x'), true},
                                    {"--unterminated", false}}));
  ASSERT_TRUE(partial.has_value());
  ASSERT_EQ(partial->argv.size(), 2u);
  EXPECT_EQ(partial->argv[1].size(), 2000u);
}

TEST_F(ProcessExtractorTest, InvalidStartPidReturnsEmptyProcessTree) {
  bool provider_called = false;
  auto provider = base::BindRepeating(
      [](bool* called,
         base::ProcessId pid) -> std::optional<internal::RawProcessInfo> {
        *called = true;
        return internal::RawProcessInfo(base::kNullProcessId,
                                        "/usr/local/bin/node",
                                        {"node", "/Users/alice/agent.js"});
      },
      &provider_called);

  EXPECT_TRUE(internal::ExtractProcessTreeWithProviderForTesting(
                  base::kNullProcessId, provider)
                  .empty());
  EXPECT_FALSE(provider_called);
  EXPECT_TRUE(ExtractProcessTreeFromPid(base::kNullProcessId).empty());
  EXPECT_EQ(internal::ResolveWebSocketPeerPidForTesting(net::IPEndPoint()),
            base::kNullProcessId);
  EXPECT_EQ(internal::ResolveWebSocketPeerPidForTesting(
                net::IPEndPoint(net::IPAddress(), 54321)),
            base::kNullProcessId);
  EXPECT_EQ(internal::ResolveWebSocketPeerPidForTesting(
                net::IPEndPoint(net::IPAddress(192, 168, 1, 10), 54321)),
            base::kNullProcessId);
  EXPECT_TRUE(ExtractProcessTreeForWebSocket(net::IPEndPoint()).empty());
  EXPECT_TRUE(
      ExtractProcessTreeForWebSocket(net::IPEndPoint(net::IPAddress(), 54321))
          .empty());
  EXPECT_TRUE(ExtractProcessTreeForWebSocket(
                  net::IPEndPoint(net::IPAddress(192, 168, 1, 10), 54321))
                  .empty());
  EXPECT_TRUE(ExtractProcessTreeForWebSocket(
                  net::IPEndPoint(net::IPAddress(8, 8, 8, 8), 9222))
                  .empty());
}

TEST_F(ProcessExtractorTest,
       EnforcesMaxAncestryDepthCycleDetectionAndPartialTreeRetention) {
  // 1. Depth cap at `kMaxProcessAncestryDepth` (16).
  auto deep_provider = base::BindRepeating(
      [](base::ProcessId pid) -> std::optional<internal::RawProcessInfo> {
        return internal::RawProcessInfo(pid + 1, "/usr/local/bin/node",
                                        {"node", "/Users/alice/worker.js"});
      });
  ProcessTree deep_tree =
      internal::ExtractProcessTreeWithProviderForTesting(100, deep_provider);
  EXPECT_EQ(deep_tree.process_nodes.size(), kMaxProcessAncestryDepth);
  EXPECT_EQ(deep_tree.process_nodes.front().script_path,
            "/Users/<USER>/worker.js");

  // 2. Cycle detection (`100 -> 200 -> 100`) and self-cycle (`500 -> 500`).
  std::map<base::ProcessId, internal::RawProcessInfo> cycle_map = {
      {100,
       internal::RawProcessInfo(200, "/usr/local/bin/node", {"node", "a.js"})},
      {200, internal::RawProcessInfo(100, "/bin/zsh", {"zsh", "b.sh"})},
      {500, internal::RawProcessInfo(500, "/usr/bin/python3", {"python3"})},
  };
  auto cycle_provider = base::BindRepeating(&LookupProcessInMap, cycle_map);
  EXPECT_EQ(
      internal::ExtractProcessTreeWithProviderForTesting(100, cycle_provider)
          .process_nodes.size(),
      2u);
  EXPECT_EQ(
      internal::ExtractProcessTreeWithProviderForTesting(500, cycle_provider)
          .process_nodes.size(),
      1u);

  // 3. Partial tree retention when parent PID 400 exits mid-walk.
  std::map<base::ProcessId, internal::RawProcessInfo> partial_map = {
      {300, internal::RawProcessInfo(400, "/Users/alice/bin/node",
                                     {"node", "/Users/alice/agent.js"})},
  };
  ProcessTree partial_tree = internal::ExtractProcessTreeWithProviderForTesting(
      300, base::BindRepeating(&LookupProcessInMap, partial_map));
  ASSERT_EQ(partial_tree.process_nodes.size(), 1u);
  EXPECT_EQ(partial_tree.process_nodes[0].executable_path,
            "/Users/<USER>/bin/node");
  EXPECT_EQ(partial_tree.process_nodes[0].script_path,
            "/Users/<USER>/agent.js");

  // 4. Overlong `KERN_PROCARGS2` paths scrub the OS home directory and check
  // leading flags before front-truncating at `kMaxProcessPathLength` (1024).
  base::ScopedPathOverride long_home(
      base::DIR_HOME, base::FilePath(FILE_PATH_LITERAL("/Users/longusername")),
      /*is_absolute=*/true, /*create=*/false);
  const std::string long_user_script =
      "/Users/longusername/" + std::string(1001, 'a') + "/agent.js";
  const std::string long_eval_flag = "--eval=" + std::string(1030, 'a') + ".js";
  std::map<base::ProcessId, internal::RawProcessInfo> long_map = {
      {600,
       internal::RawProcessInfo(base::kNullProcessId, "/usr/local/bin/node",
                                {"node", long_user_script})},
      {601,
       internal::RawProcessInfo(base::kNullProcessId, "/usr/local/bin/node",
                                {"node", long_eval_flag, "real.js"})},
  };
  ProcessTree long_user_tree =
      internal::ExtractProcessTreeWithProviderForTesting(
          600, base::BindRepeating(&LookupProcessInMap, long_map));
  ASSERT_EQ(long_user_tree.process_nodes.size(), 1u);
  EXPECT_EQ(long_user_tree.process_nodes[0].script_path,
            "/Users/<USER>/" + std::string(1001, 'a') + "/agent.js");
  EXPECT_EQ(long_user_tree.process_nodes[0].script_path.find("longusername"),
            std::string::npos);

  ProcessTree long_eval_tree =
      internal::ExtractProcessTreeWithProviderForTesting(
          601, base::BindRepeating(&LookupProcessInMap, long_map));
  ASSERT_EQ(long_eval_tree.process_nodes.size(), 1u);
  EXPECT_TRUE(long_eval_tree.process_nodes[0].script_path.empty());
}

TEST_F(ProcessExtractorTest, LiveCurrentProcessAncestryExtraction) {
  const base::ProcessId self_pid = base::GetCurrentProcId();
#if BUILDFLAG(IS_POSIX)
  ASSERT_GT(self_pid, static_cast<base::ProcessId>(1));

  ProcessTree tree = ExtractProcessTreeFromPid(self_pid);
  ASSERT_FALSE(tree.empty());
  EXPECT_LE(tree.process_nodes.size(), kMaxProcessAncestryDepth);
  EXPECT_EQ(tree.process_nodes[0].process_id, self_pid);
  EXPECT_FALSE(tree.process_nodes[0].executable_path.empty());
  EXPECT_TRUE(tree.process_nodes[0].script_path.empty());

  if (tree.process_nodes[0].parent_process_id >
      static_cast<base::ProcessId>(1)) {
    ProcessTree pipe_tree = ExtractProcessTreeForPipe();
    ASSERT_FALSE(pipe_tree.empty());
    EXPECT_EQ(pipe_tree.process_nodes[0].process_id,
              tree.process_nodes[0].parent_process_id);
  }

  EXPECT_TRUE(ExtractProcessTreeForWebSocket(net::IPEndPoint()).empty());
#else
  EXPECT_TRUE(ExtractProcessTreeFromPid(self_pid).empty());
#endif
}

}  // namespace

}  // namespace enterprise_connectors
