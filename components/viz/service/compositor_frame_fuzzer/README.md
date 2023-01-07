# CompositorFrame Fuzzer

## Fuzzer functionality

The fuzzer takes a binary protobuf RenderPass message, as specified in
compositor_frame_fuzzer.proto, which describes the root RenderPass for a
CompositorFrame as input. It uses [libFuzzer][libfuzzer] and
[libprotobuf-mutator][protobuf-mutator] to generate and mutate a corpus of
inputs that exercise as many code paths as possible. See [libFuzzer in
Chromium][libfuzzer-chromium] documentation for general information on these
tools and how they are used in Chromium.

On each iteration, the CompositorFrame fuzzer builds a CompositorFrame
following the specifications in the protobuf-format input and submits it to the
display compositor. A simulated browser process submits a CompositorFrame which
embeds the fuzzed CompositorFrame. The display compositor then produces pixel
output using SoftwareRenderer.

## Seed corpus

A seed corpus helps jumpstart the fuzzer by providing it with an existing set of
valid inputs to try. See the [efficient fuzzing guide][efficient-fuzzing] for
more information.

The `.asciipb` files in the text_format_seed_corpus directory will automatically
be compiled and added to the seed corpus once they have been added to the
sources list in BUILD.gn.

## Running locally

To run multiple fuzzer iterations and generate a corpus (the initial corpus
directory may be empty or already contain entries):

```shell
compositor_frame_fuzzer <path-to-corpus> [optional: <path-to-seed-dir>]
```

To execute a single input test case:

```shell
compositor_frame_fuzzer <path-to-test-case>
```

## Debugging

The CompositorFrame fuzzer produces minimal logging, but will accept verbosity
flags (`--v=1`) to enable helpful logging for debugging.

Since the fuzzer runs headlessly, run it with the flag
`--dump-to-png[=dir-name]` to dump the browser display into PNG files for
debugging.

A possibly useful pattern to debug new fuzzer functionality is to write a new
seed corpus entry exercising the new paths, then visually testing whether this
entry is rendered correctly. For instance, to test that the
`nested_render_pass_draw_quads.asciipb` corpus entry renders correctly:

```shell
compositor_frame_fuzzer <path-to-build-gen-files>/components/viz/service/compositor_frame_fuzzer/binary_seed_corpus/nested_render_pass_draw_quads.pb --v=1 --dump-to-png=<path-to-out-dir>
```

[libfuzzer]: http://llvm.org/docs/LibFuzzer.html
[protobuf-mutator]: https://github.com/google/libprotobuf-mutator/
[libfuzzer-chromium]: https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/README.md
[efficient-fuzzing]: https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/efficient_fuzzer.md
