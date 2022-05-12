The PDF component contains code necessary for using the PDF plugin in
content-based clients. The PDF plugin code that lives in `//pdf` cannot depend
on `//content` directly, so it uses a variety of delegate interfaces which are
implement here. This component also contains code shared among content-based
clients that should not live in `//chrome`.
