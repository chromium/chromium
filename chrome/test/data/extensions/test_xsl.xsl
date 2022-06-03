<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:template match="/">
    <html>
      <head>
        <script>
          async function foo() {
            await new Promise(r => setTimeout(r, 2000));
          }
          foo();
        </script>
      </head>
      <body>
        <h2>Don't Panic</h2>
        <table border="1">
          <tr>
            <th>Title</th>
            <th>Author</th>
            <th>Publication Date</th>
          </tr>
          <xsl:for-each select="catalog/book">
            <tr>
              <td><xsl:value-of select="title"/></td>
              <td><xsl:value-of select="author"/></td>
              <td><xsl:value-of select="publish_date"/></td>
            </tr>
          </xsl:for-each>
        </table>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
