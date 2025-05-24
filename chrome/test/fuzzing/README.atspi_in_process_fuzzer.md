So, you've had a bug reported by `atspi_in_process_fuzzer`? What does this mean?

This fuzzer attempts to explore the Chrome UI using the same accessibility APIs
as a Linux screen reader.

If this fuzzer has found a bug, it means:

* The bug can definitely be reached by using the Linux screen reader APIs.
  Like other fuzzer bugs, it may not be reproducible due to race conditions
  etc., but it can theoretically be hit this way.
* The bug can possibly by reached by Linux screen readers. (While they use these
  APIs, they may use them in a more limited and predictable way than real
  existing screen readers).
* The bug can quite possibly be reached by a user interacting with the regular
  Chrome API, without using any accessibility APIs at all.

## Can I reproduce the bug using the fuzzer?

Maybe. You should find that the ClusterFuzz report contains a link to a protobuf
like this:

```
action {
  path_to_control {
    path_to_control {
      anonymous {
        role: "frame"
      }
    }
    path_to_control {
      named {
        name: "Experiments"
      }
    }
  }
  verb {
    take_action {
      action_id: 1
    }
  }
}
```

You may be able to build the fuzzer and then run it giving it this test case
file on the command line. Instructions for building fuzzers are beyond the scope
of this - see our [general fuzzing documentation](/testing/libfuzzer/reproducing.md).

There's a chance that this won't work because the nature of this fuzzer is that
it tends to accumulate UI state before eventually finding a problem.

## How else can I try to reproduce it?

You may be able to reproduce it by:

* Taking the same actions using the Linux
  [accerciser](https://help.gnome.org/users/accerciser/stable/introduction.html.en)
  tool. The contents of the protobuf should match precisely to the tree of
  controls exposed in accerciser, and the actions to be taken should be
  relatively obvious.
* Or even, just figuring out what the controls are, and clicking on them
  manually in the Chrome UI. Often bugs involve unexpected interactions between
  multiple controls; you might find that the protobuf suggests that fiddling
  with (say) Tab Groups and then Bookmarks might yield the bug.

## What if I can't reproduce it?

As ever, please figure out if you can work out the root cause from the crash
information. The protobuf describing the controls involved should also help.

## Are these security bugs?

Maybe. If (for instance) a use-after-free can be reached only via UI
interaction, it's not an especially _attractive_ bug for attackers, but they
might still be able to exploit it by enticing the user to take these steps in
the UI. Chrome's [security severity guidelines](/docs/security/severity-guidelines.md)
reflect this by bumping down bug severity by one or more levels if UI steps
are required - but still considering such bugs to be security bugs.
