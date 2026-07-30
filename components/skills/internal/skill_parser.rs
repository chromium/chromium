// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace = "skills::ffi")]
mod ffi {
    struct SkillParseResult {
        name: String,
        description: String,
        prompt: String,
        success: bool,
    }

    extern "Rust" {
        fn parse_skill_yaml_frontmatter(content: &str) -> SkillParseResult;
    }
}

pub fn parse_skill_yaml_frontmatter(content: &str) -> ffi::SkillParseResult {
    let mut name = String::new();
    let mut description = String::new();
    let mut prompt = String::new();
    let mut success = false;

    // Normalize Windows-style newlines to Unix-style to ensure consistent
    // splitting. Stray carriage returns ('\r') are intentionally left intact to
    // break extraction for invalid or malformed line endings as expected by the
    // specification.
    let content_normalized = content.replace("\r\n", "\n");

    // Split on the boundary separating the YAML frontmatter from the prompt body.
    if let Some((frontmatter, body)) = content_normalized.split_once("\n---") {
        let frontmatter = frontmatter.trim_start_matches("---").trim();
        // The body might start with an optional newline which we strip, then trim.
        prompt = body.trim_start_matches('\n').trim().to_string();

        for line in frontmatter.lines() {
            let line = line.trim();
            if let Some((key, val)) = line.split_once(":") {
                let key = key.trim();
                let val = val.trim().trim_matches(|c| c == ' ' || c == '"' || c == '\'');
                match key {
                    "name" => name = val.to_string(),
                    "description" => description = val.to_string(),
                    _ => {}
                }
            }
        }

        if !name.is_empty() {
            success = true;
        }
    }

    ffi::SkillParseResult { name, description, prompt, success }
}
