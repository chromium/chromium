### Top Domains Utilities

* `domains.list`

  A top domain list, one per line. Used as an input to
  make_top_domain_skeletons. See http://go/chrome-top-domains-update for update
  instructions.

  This list can contain ASCII and unicode domains. Unicode domains should not be
  encoded in punycode.


* `domains.skeletons`

  The checked-in output of make_top_domain_skeletons.  Processed during the
  build to generate domains-trie-inc.cc, which is used by
  idn_spoof_checker.cc.  This must be regenerated as follows if ICU is updated,
  since skeletons can differ across ICU versions:

  $ ninja -C $build_outdir make_top_domain_skeletons
  $ $build_outdir/make_top_domain_skeletons

* `test_domains.list`

  A list of domains to use in IDNToUnicode test instead of the actual
  top domain list. Manually edited to match what's in IDNToUnicode test.

* `test_domains.skeletons`

  Generated output of test_domains.list along with domains.skeletons
  by make_top_domain_skeletons.

* `top_domain_generator.cc`

  Generates the Huffman encoded Trie containing a map of skeletons to top
  domains. For now, the skeletons must be ASCII. Unicode domains are supported
  but they are written as punycode to the trie.

* `top_domain_list_variable_builder.cc` / `top500_domains.h`

  `top_domain_list_variable_builder.cc` is run at compile time to generate
  information about the top 500 domains (currently, skeletons and keywords are
  created from these domains). This information is then embedded directly into
  the chrome binary, and can be accessed via the variables in the top500_domains
  namespace.
