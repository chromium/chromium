// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace = "reading_mode")]
mod ffi {
    struct DistillationMetrics {
        rouge_l_precision: f32,
        rouge_l_recall: f32,
        rouge_l_f1: f32,
        rouge_l_f2: f32,
        struct_score: f32,
        format_score: f32,
        link_density_ratio: f32,
        well_formed: bool,
    }

    struct OriginalList {
        items: Vec<String>,
    }

    struct TaggedText {
        tag_path: Vec<String>,
        text: String,
    }

    struct OriginalStructure {
        headings: Vec<String>,
        lists: Vec<OriginalList>,
        bold_fragments: Vec<String>,
        italic_fragments: Vec<String>,
        code_fragments: Vec<String>,
        blockquotes: Vec<String>,
        original_link_text_len: u32,
        original_total_text_len: u32,
    }

    extern "Rust" {
        fn parse_distilled_html(html: &str) -> Vec<TaggedText>;
        fn evaluate(
            original_text: &str,
            distilled_html: &str,
            structure: OriginalStructure,
        ) -> DistillationMetrics;
    }
}

fn lcs_words(a: &str, b: &str) -> usize {
    let a_words: Vec<&str> = a.split_whitespace().collect();
    let b_words: Vec<&str> = b.split_whitespace().collect();
    let m = a_words.len();
    let n = b_words.len();
    if m == 0 || n == 0 {
        return 0;
    }

    // Space-optimized LCS using two rows.
    let mut prev_row = vec![0; n + 1];
    let mut curr_row = vec![0; n + 1];

    for i in 1..=m {
        for j in 1..=n {
            if a_words[i - 1] == b_words[j - 1] {
                curr_row[j] = prev_row[j - 1] + 1;
            } else {
                curr_row[j] = std::cmp::max(prev_row[j], curr_row[j - 1]);
            }
        }
        std::mem::swap(&mut prev_row, &mut curr_row);
    }
    prev_row[n]
}

use ffi::TaggedText;

/// Parses and tokenizes distilled HTML to extract non-empty text segments
/// mapped to their active node tag hierarchy (tag path).
///
/// This parser implements a lightweight state-machine based HTML tokenizer
/// optimized for distilled readability views.
///
/// ### Core Algorithm:
/// The function scans the HTML input sequentially character-by-character:
/// 1. **State Tracking:**
///    - `in_tag`: Tracked to check if we are reading HTML tag metadata (`<...>`
///      markup) vs raw text.
///    - `in_quotes`: Tracked when inside attribute values (e.g. `href="..."`),
///      preventing characters like `>` inside quotes from prematurely
///      terminating the tag context.
///    - `tag_stack`: Keeps track of the nested list of active tags enclosing
///      the text currently being parsed.
/// 2. **Tag Discovery & Stack Mutation:**
///    - Transitioning into a tag (`<`) triggers flushing any accumulated text
///      to the output list.
///    - Encountering standard tag endings (`>`) ends tag context. If the tag
///      name represents a closing tag (`/TAG`), the parser finds the last
///      matching parent element in `tag_stack` and truncates the stack. If it's
///      an opening tag, it is pushed to `tag_stack` unless identified as a
///      void/self-closing element.
/// 3. **Text Flushing:**
///    - Accumulates content between elements and appends a `TaggedText`
///      representing the text segment alongside a snapshot copy of the active
///      `tag_stack` sequence at extraction time.
///
/// ### Supported Edge Cases:
/// - **Unescaped Brackets:** Meets `<` that are not followed by an ASCII
///   alphabetic or `/` character (e.g., in expressions like `a < b`), parsing
///   them as literal text.
/// - **Attributes with Quotes:** Gracefully skips tag parameters and respects
///   quotes, so characters like `>` inside attribute strings (e.g., `<img
///   alt="next >">`) are not misidentified as tag delimiters.
/// - **Void & Self-Closing Elements:** Standard structural void tags (e.g.
///   `BR`, `HR`, `IMG`) and self-closing tags (ending with `/`) are recognized
///   and immediately popped/ignored instead of remaining on the tag stack.
/// - **Case Insensitivity:** All tag names (e.g. `<h1>` vs `<H1>`) are
///   normalized to uppercase in the tag path.
///
/// ### Limits & Unsupported Edge Cases:
/// - **Malformed Tags / Missing Closers:** An unclosed tag (e.g., `<li>`
///   without a matching `</li>`) will remain on the tag stack indefinitely,
///   incorrectly applying to all subsequent text.
/// - **Entity Decoding:** HTML entities (e.g., `&amp;`) are parsed as literal
///   strings.
/// - **Script/Style Contents:** Content inside `<script>` or `<style>` tags is
///   not filtered and is processed as text (assumed to be stripped upstream).
///
/// ### Return Value:
/// Returns a list of `TaggedText` elements, containing the extracted non-empty
/// text strings and their enclosing uppercase HTML tag paths (e.g. `["DIV",
/// "P", "B"]`).
fn parse_distilled_html(html: &str) -> Vec<TaggedText> {
    parse_distilled_html_internal(html).0
}

/// Represents a tokenized segment of an HTML stream.
enum HtmlSegment<'a> {
    /// Content inside `<...>` markup (excluding the `<` and `>` delimiters).
    Tag(&'a str),
    /// A single character of plain text outside of tag metadata.
    TextChar(char),
}

/// Scans an HTML string character-by-character and dispatches extracted
/// segments to a handler.
///
/// Centralizes HTML tag bounds detection and state management across parsing
/// utilities:
/// - Tracks attribute quotes (`in_quotes`) so `>` characters inside attribute
///   values (e.g., `alt="a > b"`) do not prematurely terminate tag extraction.
/// - Peeks ahead after encountering `<` (`is_ascii_alphabetic()` or `/`) to
///   ensure unescaped `<` characters (e.g., mathematical expressions like `a <
///   b`) are treated as plain text.
fn get_next_tag_segment(html: &str, mut on_segment: impl FnMut(HtmlSegment<'_>)) {
    let mut current_tag = String::new();
    let mut in_tag = false;
    let mut in_quotes: Option<char> = None;
    let mut chars = html.chars().peekable();

    while let Some(c) = chars.next() {
        if in_tag {
            if let Some(q) = in_quotes {
                if c == q {
                    in_quotes = None;
                }
                current_tag.push(c);
            } else if c == '>' {
                in_tag = false;
                on_segment(HtmlSegment::Tag(&current_tag));
                current_tag.clear();
            } else {
                if c == '"' || c == '\'' {
                    in_quotes = Some(c);
                }
                current_tag.push(c);
            }
        } else if c == '<' {
            if let Some(&next_c) = chars.peek()
                && (next_c.is_ascii_alphabetic() || next_c == '/')
            {
                in_tag = true;
                current_tag.clear();
                continue;
            }
            on_segment(HtmlSegment::TextChar(c));
        } else {
            on_segment(HtmlSegment::TextChar(c));
        }
    }
}

fn parse_distilled_html_internal(html: &str) -> (Vec<TaggedText>, bool) {
    let mut result = Vec::new();
    let mut tag_stack: Vec<String> = Vec::new();
    let mut current_text = String::new();
    let mut well_formed = true;

    let flush_text = |text: &mut String, stack: &[String], res: &mut Vec<TaggedText>| {
        let trimmed = text.trim();
        if !trimmed.is_empty() {
            res.push(TaggedText { tag_path: stack.to_vec(), text: trimmed.to_string() });
        }
        text.clear();
    };

    // Handler that accumulates text characters into segments and mutates
    // `tag_stack` when encountering structural tag openings or closings.
    let extract_tagged_text_on_segment = |segment: HtmlSegment<'_>| match segment {
        HtmlSegment::Tag(tag) => {
            flush_text(&mut current_text, &tag_stack, &mut result);
            let tag_trimmed = tag.trim();
            if let Some(stripped) = tag_trimmed.strip_prefix('/') {
                let name = stripped.trim().to_uppercase();
                if let Some(pos) = tag_stack.iter().rposition(|x| x == &name) {
                    // If the closing tag does not match the very top of the stack,
                    // it means an inner tag was left unclosed.
                    if pos != tag_stack.len() - 1 {
                        well_formed = false;
                    }
                    tag_stack.truncate(pos);
                } else {
                    // Closing tag with no matching opening tag.
                    well_formed = false;
                }
            } else {
                let name = tag_trimmed.split_whitespace().next().unwrap_or("").to_uppercase();
                // Check if it's a void element or self-closing
                let is_void = matches!(
                    name.as_str(),
                    "BR" | "HR"
                        | "IMG"
                        | "INPUT"
                        | "META"
                        | "LINK"
                        | "COL"
                        | "EMBED"
                        | "PARAM"
                        | "SOURCE"
                        | "TRACK"
                        | "WBR"
                );
                let is_self_closing = tag_trimmed.ends_with('/') || is_void;

                if !name.is_empty() && !is_self_closing {
                    tag_stack.push(name);
                }
            }
        }
        HtmlSegment::TextChar(c) => {
            current_text.push(c);
        }
    };

    get_next_tag_segment(html, extract_tagged_text_on_segment);

    flush_text(&mut current_text, &tag_stack, &mut result);
    if !tag_stack.is_empty() {
        well_formed = false;
    }
    (result, well_formed)
}

/// Strips HTML tags from the given string, replacing them with a space
/// separator. Handles unescaped bracket characters and quoted strings inside
/// tag parameters.
fn strip_tags(html: &str) -> String {
    let mut result = String::new();

    // Handler that appends plain text characters and converts HTML tag boundaries
    // into space separators to prevent word concatenation.
    let strip_tags_on_segment = |segment: HtmlSegment<'_>| match segment {
        HtmlSegment::Tag(_) => {
            // Add a space to prevent word concatenation, unless the result already ends
            // with space.
            if !result.ends_with(' ') {
                result.push(' ');
            }
        }
        HtmlSegment::TextChar(c) => {
            result.push(c);
        }
    };

    get_next_tag_segment(html, strip_tags_on_segment);

    result
}

/// Calculates total character count and link-specific character count in the
/// stripped version of a distilled HTML document.
fn calculate_link_density(html: &str) -> (usize, usize) {
    let mut total_len = 0;
    let mut link_len = 0;
    let mut in_link = false;

    // Handler that tracks active `<A>`/`</A>` link context and records character
    // counts.
    let compute_link_density_on_segment = |segment: HtmlSegment<'_>| match segment {
        HtmlSegment::Tag(tag) => {
            let tag_name = tag.split_whitespace().next().unwrap_or("").to_uppercase();
            if tag_name == "A" {
                in_link = true;
            } else if tag_name == "/A" {
                in_link = false;
            }
        }
        HtmlSegment::TextChar(_) => {
            total_len += 1;
            if in_link {
                link_len += 1;
            }
        }
    };

    get_next_tag_segment(html, compute_link_density_on_segment);

    (link_len, total_len)
}

// Calculates distillation quality metrics by comparing the distilled HTML
// against the original content and AXTree counts. Preservation ratios are
// capped at 1.0 to prevent over-scoring if distillation duplicates elements.

fn calculate_rouge_l(original_text: &str, stripped_html: &str) -> (f32, f32, f32, f32) {
    let lcs = lcs_words(original_text, stripped_html) as f32;
    let original_words_count = original_text.split_whitespace().count() as f32;
    let distilled_words_count = stripped_html.split_whitespace().count() as f32;

    let recall = if original_words_count > 0.0 { lcs / original_words_count } else { 0.0 };
    let precision = if distilled_words_count > 0.0 { lcs / distilled_words_count } else { 0.0 };
    let f1 = if precision + recall > 0.0 {
        2.0 * (precision * recall) / (precision + recall)
    } else {
        0.0
    };
    let f2 = if 4.0 * precision + recall > 0.0 {
        5.0 * (precision * recall) / (4.0 * precision + recall)
    } else {
        0.0
    };
    (precision, recall, f1, f2)
}

fn calculate_struct_score(
    structure: &ffi::OriginalStructure,
    tagged_nodes: &[TaggedText],
    distilled_text_lower: &str,
) -> f32 {
    // Heading Alignment Metric
    let dist_headings: Vec<&TaggedText> = tagged_nodes
        .iter()
        .filter(|node| {
            node.tag_path.iter().any(|t| {
                t.len() == 2
                    && t.starts_with('H')
                    && ('1'..='6').contains(&t.chars().nth(1).unwrap())
            })
        })
        .collect();

    let mut expected_headings = 0;
    let mut matched_headings = 0;
    for orig_h in &structure.headings {
        let orig_h_clean = orig_h.trim().to_lowercase();
        if distilled_text_lower.contains(&orig_h_clean) {
            expected_headings += 1;
            if dist_headings.iter().any(|dh| {
                let dh_clean = dh.text.trim().to_lowercase();
                dh_clean.contains(&orig_h_clean) || orig_h_clean.contains(&dh_clean)
            }) {
                matched_headings += 1;
            }
        }
    }

    let heading_ratio = if expected_headings > 0 {
        matched_headings as f32 / expected_headings as f32
    } else {
        1.0
    };

    // Grouped Nested List Metric
    let mut dist_lists: Vec<Vec<String>> = Vec::new();
    let mut current_list: Vec<String> = Vec::new();
    let mut in_list = false;

    for node in tagged_nodes {
        let is_li = node.tag_path.iter().any(|i| i == "LI");
        if is_li {
            in_list = true;
            current_list.push(node.text.trim().to_string());
        } else if in_list {
            if !current_list.is_empty() {
                dist_lists.push(current_list.clone());
                current_list.clear();
            }
            in_list = false;
        }
    }
    if !current_list.is_empty() {
        dist_lists.push(current_list);
    }

    let mut expected_lists = 0;
    let mut total_list_score = 0.0;
    if !structure.lists.is_empty() {
        for orig_list in &structure.lists {
            let expected_items: Vec<&String> = orig_list
                .items
                .iter()
                .filter(|item| distilled_text_lower.contains(&item.trim().to_lowercase()))
                .collect();

            if !expected_items.is_empty() {
                expected_lists += 1;
                let mut best_list_match = 0.0;
                for dist_list in &dist_lists {
                    let mut matched_items = 0;
                    for orig_item in &expected_items {
                        let orig_item_clean = orig_item.trim().to_lowercase();
                        if dist_list.iter().any(|di| {
                            let di_clean = di.to_lowercase();
                            di_clean.contains(&orig_item_clean)
                                || orig_item_clean.contains(&di_clean)
                        }) {
                            matched_items += 1;
                        }
                    }
                    let match_ratio = matched_items as f32 / expected_items.len() as f32;
                    if match_ratio > best_list_match {
                        best_list_match = match_ratio;
                    }
                }
                total_list_score += best_list_match;
            }
        }
    }

    let list_completeness =
        if expected_lists > 0 { total_list_score / expected_lists as f32 } else { 1.0 };

    (heading_ratio + list_completeness) / 2.0
}

fn calculate_format_score(
    structure: &ffi::OriginalStructure,
    tagged_nodes: &[TaggedText],
    distilled_text_lower: &str,
) -> f32 {
    // Content-Aligned Format Metric
    let dist_bold: Vec<&TaggedText> = tagged_nodes
        .iter()
        .filter(|node| {
            node.tag_path.iter().any(|i| i == "B") || node.tag_path.iter().any(|i| i == "STRONG")
        })
        .collect();

    let dist_italic: Vec<&TaggedText> = tagged_nodes
        .iter()
        .filter(|node| {
            node.tag_path.iter().any(|i| i == "I") || node.tag_path.iter().any(|i| i == "EM")
        })
        .collect();

    let dist_code: Vec<&TaggedText> =
        tagged_nodes.iter().filter(|node| node.tag_path.iter().any(|i| i == "CODE")).collect();

    let dist_blockquotes: Vec<&TaggedText> = tagged_nodes
        .iter()
        .filter(|node| node.tag_path.iter().any(|i| i == "BLOCKQUOTE"))
        .collect();

    let calculate_format_preservation =
        |orig_frags: &[String], dist_frags: &[&TaggedText], target_text: &str| -> f32 {
            let expected_frags: Vec<&String> = orig_frags
                .iter()
                .filter(|frag| target_text.contains(&frag.trim().to_lowercase()))
                .collect();

            if expected_frags.is_empty() {
                return 1.0;
            }

            let mut matched = 0;
            for orig in &expected_frags {
                let orig_clean = orig.trim().to_lowercase();
                if dist_frags.iter().any(|df| {
                    let df_clean = df.text.trim().to_lowercase();
                    df_clean.contains(&orig_clean) || orig_clean.contains(&df_clean)
                }) {
                    matched += 1;
                }
            }
            matched as f32 / expected_frags.len() as f32
        };

    let bold_score =
        calculate_format_preservation(&structure.bold_fragments, &dist_bold, distilled_text_lower);
    let italic_score = calculate_format_preservation(
        &structure.italic_fragments,
        &dist_italic,
        distilled_text_lower,
    );
    let code_score =
        calculate_format_preservation(&structure.code_fragments, &dist_code, distilled_text_lower);
    let blockquote_score = calculate_format_preservation(
        &structure.blockquotes,
        &dist_blockquotes,
        distilled_text_lower,
    );

    (bold_score + italic_score + code_score + blockquote_score) / 4.0
}

fn calculate_link_density_ratio(structure: &ffi::OriginalStructure, distilled_html: &str) -> f32 {
    // Calculate the density reduction of the distilled HTML.
    // 0.0 density implies no reduction, while a 1.0 implies complete reduction.
    let (dist_link_len, dist_total_len) = calculate_link_density(distilled_html);
    let dist_density =
        if dist_total_len > 0 { dist_link_len as f32 / dist_total_len as f32 } else { 0.0 };
    let orig_density = if structure.original_total_text_len > 0 {
        structure.original_link_text_len as f32 / structure.original_total_text_len as f32
    } else {
        0.0
    };

    if orig_density > 0.0 {
        f32::max(0.0, (orig_density - dist_density) / orig_density)
    } else if dist_density == 0.0 {
        1.0
    } else {
        0.0
    }
}

fn evaluate(
    original_text: &str,
    distilled_html: &str,
    structure: ffi::OriginalStructure,
) -> ffi::DistillationMetrics {
    let stripped_html = strip_tags(distilled_html);
    let distilled_text_lower = stripped_html.to_lowercase();
    let (tagged_nodes, well_formed) = parse_distilled_html_internal(distilled_html);

    let (rouge_l_precision, rouge_l_recall, rouge_l_f1, rouge_l_f2) =
        calculate_rouge_l(original_text, &stripped_html);

    let struct_score = calculate_struct_score(&structure, &tagged_nodes, &distilled_text_lower);
    let format_score = calculate_format_score(&structure, &tagged_nodes, &distilled_text_lower);
    let link_density_ratio = calculate_link_density_ratio(&structure, distilled_html);

    ffi::DistillationMetrics {
        rouge_l_precision,
        rouge_l_recall,
        rouge_l_f1,
        rouge_l_f2,
        struct_score,
        format_score,
        link_density_ratio,
        well_formed,
    }
}
