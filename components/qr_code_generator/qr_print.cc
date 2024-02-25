// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This helper binary can be compiled to aid in development / debugging of the
// QR generation code. It prints a QR code to the console and thus allows much
// faster iteration. It is not built by default, see the BUILD.gn in this
// directory.

#include <stdio.h>

#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "components/qr_code_generator/qr_code_generator.h"

// kTerminalBackgroundIsBright controls the output polarity. Many QR scanners
// will cope with inverted bright / dark but, if you have a bright terminal
// background, you may need to change this.
constexpr bool kTerminalBackgroundIsBright = false;

// kPaint is a pair of UTF-8 encoded code points for U+2588 ("FULL BLOCK").
static constexpr char kPaint[] = "\xe2\x96\x88\xe2\x96\x88";
static constexpr char kNoPaint[] = "  ";

static void PrintHorizontalLine(const char* white, int size) {
  for (int x = 0; x < size + 2; x++) {
    fputs(white, stdout);
  }
  fputs("\n", stdout);
}

int main(int argc, char** argv) {
  // Presubmits don't allow fprintf to a variable called |stderr|.
  FILE* const STDERR = stderr;

  if (argc < 2 || argc > 3) {
    fprintf(STDERR, "Usage: %s <input string> [mask number]\n", argv[0]);
    return 1;
  }

  const uint8_t* const input = reinterpret_cast<const uint8_t*>(argv[1]);
  const size_t input_len = strlen(argv[1]);

  std::optional<uint8_t> mask;
  if (argc == 3) {
    unsigned mask_unsigned;
    if (!base::StringToUint(argv[2], &mask_unsigned) || mask_unsigned > 7) {
      fprintf(STDERR, "Mask numbers run from zero to seven.\n");
      return 1;
    }
    mask = static_cast<uint8_t>(mask_unsigned);
  }

  const char* black = kNoPaint;
  const char* white = kPaint;
  if (kTerminalBackgroundIsBright) {
    std::swap(black, white);
  }

  auto code = qr_code_generator::GenerateCode(
      base::span<const uint8_t>(input, input_len), mask);
  if (!code.has_value()) {
    fprintf(STDERR, "Input too long to be encoded.\n");
    return 2;
  }

  const int size = code->qr_size;
  PrintHorizontalLine(white, size);

  int i = 0;
  for (int y = 0; y < size; y++) {
    fputs(white, stdout);
    for (int x = 0; x < size; x++) {
      fputs((code->data[i++] & 1) ? black : white, stdout);
    }
    fputs(white, stdout);
    fputs("\n", stdout);
  }

  PrintHorizontalLine(white, size);

  return 0;
}
